# hcproxy
**hcproxy** is a lightweight forward HTTP proxy that implements just one HTTP method -- `CONNECT`. It can be used to proxy HTTPS (but not unencrypted HTTP) traffic.

## Requirements

*  To compile: C++17 compiler.
*  To run: Linux, `libc`.
*  To run as daemon: `system.d`.

## Compiling

```shell
git clone git@github.com:romkatv/hcproxy.git
cd hcproxy
make
```

## Installing locally

```shell
sudo make install
```

## Installing remotely via SSH

```shell
./install-via-ssh certificate.pem user@12.34.56.78
```

## Configuring

To change configuration, you'll need to modify the source code of `main()` in `hcproxy.cc` and recompile. For example, here's how you can change the port on which `hcproxy` listens and the timeout for establishing outgoing connections.

```diff
   hcproxy::Options opt;
+  opt.listen_port = 1234;
+  opt.connect_timeout = std::chrono::seconds(10);
   hcproxy::RunProxy(opt);
```

The list of options, their descriptions and default values can be found in the source code.
