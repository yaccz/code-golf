package golang

import "runtime"
import "time"
import "github.com/google/uuid"
import "github.com/prometheus/client_golang/prometheus/push"
import "github.com/bensallen/modscan/pkg/uname"

type Instance struct {
	Runtime    *Runtime
	Config     *Config
	Prometheus *push.Pusher
}

func NewInstance() (*Instance, error) {
	runtime, err := NewRuntime()
	if err != nil {
		return nil, err
	}
	i := new(Instance)
	i.Runtime = runtime
	c, err := NewConfig()
	if err != nil {
		return nil, err
	}
	i.Config = c

	i.Prometheus = NewPusher()
	return i, nil
}

type Runtime struct {
	ctime        time.Time
	uuid         uuid.UUID
	lang         string
	lang_version string
	runtime      string
	os           string
	kernel       string
	arch         string
}

func NewRuntime() (*Runtime, error) {
	uuid, err := uuid.NewUUID()
	if err != nil {
		return nil, err
	}

	uname, err := uname.New()
	if err != nil {
		return nil, err
	}

	r := &Runtime{
		time.Now(),
		uuid,
		"golang",
		runtime.Version(),
		"golang",
		runtime.GOOS,
		uname.Release(),
		runtime.GOARCH,
	}

	return r, nil
}
