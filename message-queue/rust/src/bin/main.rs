use std::process::exit;
use std::sync::Arc;
use std::sync::Barrier;
use std::sync::mpsc::channel;
use std::thread;
use std::error;
use std::time::Duration;

use jmcgmqp::{Instance,test_cmd,WorkerResult,Results,worker,SampleDesc};

fn sample_workers(app: &Instance, n: u64) -> Result<Results,Box<dyn error::Error>> {
    let mut workers = Vec::new();
    let mut quit_sig_senders = Vec::new();

    let barrier = Arc::new(Barrier::new(n as usize + 1));

    for i in 0..n {
        let (tx, rx) = channel();
        quit_sig_senders.push(tx);
        workers.push(worker::new(i, rx, Arc::clone(&barrier)));
    }

    let b = Arc::clone(&barrier);
    b.wait();

    thread::sleep(Duration::from_secs(app.config.duration));

    for s in quit_sig_senders {
        let _ = s.send(true);
        // if we get an error, it means client disconnected; ignore.
    }

    let mut wresults: Vec<WorkerResult> = Vec::new();
    for v in workers {
        let r = v.join().unwrap();
        if let Err(e) = r {
            return Err(Box::new(e));
        }
        let wr = r.unwrap();
        println!("{}", wr.messages_total);

        app.prometheus.messages_total.set(wr.messages_total.clone() as i64);
        app.prometheus.messages_per_second.set(wr.messages_per_second.clone());
        app.prometheus.duration_seconds.set(wr.duration.as_secs_f64().clone());

        wresults.push(wr);
        let sdesc = SampleDesc{
            n_workers: n,
            algorithm: "threading".to_string(),
            mq_system: "postgres".to_string(),
        };
        app.prometheus.push(sdesc)?;
    }
    let results = Results::new(wresults);
    return Ok(results);
}

fn main() -> Result<(), Box<dyn error::Error>> {
    let app = Instance::new()?;
    if app.config.test_prometheus == 1 {
        return test_cmd(&app);
    }
    let mut prev:Option<Results> = None;
    let mut i = 0;
    let base = 2 as u64;
    for i in 1.. {
        let pow = base.checked_pow(i);
        if pow.is_none() {
            eprintln!("Ran out of u64 powers");
            exit(1);
        }
        let rs = sample_workers(&app, pow.unwrap())?;
        println!(
            "Total: {}\nips: {}\n", rs.messages_total, rs.messages_per_second
        );
        if prev.is_some()
        && prev.as_ref().unwrap().messages_per_second >= rs.messages_per_second
        {
            break;
        } else {
            prev = Some(rs);
        }
    }

    let max = base.pow(i) as u32;
    i -= 1;

    for i in i..max {
        let rs = sample_workers(&app, i as u64)?;
        println!(
            "Total: {}\nips: {}\n", rs.messages_total, rs.messages_per_second
        );
        if prev.is_some()
        && prev.as_ref().unwrap().messages_per_second >= rs.messages_per_second
        {
            break;
        } else {
            prev = Some(rs);
        }
    }
    return Ok(())
}
