# hcproxy
**hcproxy** is a lightweight forward HTTP proxy that implements just one HTTP method -- `CONNECT`.

With decent network drivers tunneling is zero copy, which makes `hcproxy` fast and efficient. The price for this is 6 file descriptors per connection (client socket, server socket and two pipes).

## Requirements

*  To compile: C++17 compiler.
*  To run: Linux, `libc`.
*  To run as daemon: `systemd`.

## Compiling

```shell
git clone git@github.com:romkatv/hcproxy.git
cd hcproxy
make
```

## Installing locally

Install `hcproxy` as a `systemd` service:
```shell
sudo make install
```

Verify that the service is running:
```console
$ systemctl status hcproxy
● hcproxy.service - HTTP CONNECT proxy
   Loaded: loaded (/lib/systemd/system/hcproxy.service; enabled; vendor preset: enabled)
   Active: active (running) since Fri 2018-07-13 13:02:09 UTC; 30s ago
   ...
```

Try it out:
```console
$ curl -x localhost:8889 https://www.google.com/
<!doctype html>...
```

If you want to fetch an `http` URL (rather than `https`), use `-p` to force `curl` to use `CONNECT` for proxying all traffic:
```console
$ curl -p -x localhost:8889 http://www.google.com/
<!doctype html>...
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
+  opt.connect_timeout = std::chrono::seconds(30);
   hcproxy::RunProxy(opt);
```

The list of options, their descriptions and default values can be found in the source code.

The behavior of `hcproxy` cannot be customized through request headers. It simply ignores all headers.

## Using `hcproxy` as web browser proxy

You can use `hcproxy` as web browser proxy. However, unless you can convince your browser to tunnel all traffic via HTTP `CONNECT`, fetching plain `http` URLs won't work. WebSocket (`ws` and `wss` protocols) and `https` will work fine as they always go through `CONNECT`.

## Troubleshooting

If `hcproxy` doesn't like an incoming request (e.g., it's not a `CONNECT`) or cannot connect to the downstream server, it simply closes the incoming connection. It never replies with an HTTP error. The only response it ever sends to the client is HTTP 200.

The only time `hcproxy` produces output is immediately before an abnormal termination (e.g., a crash). This diagnostic is sent to `stderr`. `hcproxy` doesn't write logs.

If you've installed `htproxy` as `systemd` service, you can read high-level service logs with `journalctl`. Start and stop events, as well as crashes, should be recorded there:

```shell
journalctl -u hcproxy | tail
```
