env-build:
	docker build -t tp-env -f Dockerfile.environment .

env-windows:
	for /f "usebackq tokens=*" %%a in (`echo %cd%`) do docker run -v %%a:/home/workspace --name tp -ti tp-env

build:
	mkdir build
	gcc -o build/main src/main.c