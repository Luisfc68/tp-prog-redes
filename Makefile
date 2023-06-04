env-build:
	docker build -t tp-env -f Dockerfile.environment .

env-windows:
	for /f "usebackq tokens=*" %%a in (`echo %cd%`) do docker run -v %%a:/home/workspace --name tp -ti -p 8080:8080 tp-env

build-server:
	mkdir -p build
	gcc -o build/server src/server/main.c -pthread

build-client:
	mkdir -p build
	gcc -o build/client src/client/main.c

clean:
	rm -rf build

run-server: clean build-server
	./build/server

run-client: clean build-client
	./build/client