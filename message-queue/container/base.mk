.DEFAULT_GOAL := build

.PHONY: build
build: ## Build the image

	podman build -t localhost/$(NAME) ./

.PHONY: pull
pull: ## Pull the base image

	podman pull $(BASE_NAME)

.PHONY: run
run: ## Run the container for local development

	podman run --name $(NAME) -p 127.0.0.1:$(PORT):$(PORT) $(RUN_NAME)

.PHONY: podrun
podrun: ## Run the container in a pod

	podman run -d --name $(NAME) --pod mq $(RUN_NAME)

.PHONY: rm
rm: ## Delete container

	podman rm $(NAME)

.PHONY: help
help: ## Print help

	@@grep -h '^\([a-zA-Z]\+\):' $(MAKEFILE_LIST) | \
		sort | \
		awk -F ':.*?## ' 'NF==2 {printf "  %-26s%s\n", $$1, $$2}'
