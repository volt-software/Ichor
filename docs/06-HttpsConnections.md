# Https Connections

Ichor supports setting up and connecting to servers using SSL.

## Host

### Create certificate

First, let's create a certificate without a password:

```shell
$ openssl req -newkey rsa:2048 -new -nodes -keyout key.pem -out csr.pem -x509 -days 365 -subj "/C=NL/L=Amsterdam/O=Ichor/CN=www.example.com"
$ cat csr.pem
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIUQEtUTmX2HbHwOpDPgFkYx66ei/swDQYJKoZIhvcNAQEL
BQAwSzELMAkGA1UEBhMCTkwxEjAQBgNVBAcMCUFtc3RlcmRhbTEOMAwGA1UECgwF
SWNob3IxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAeFw0yMzEwMDIwODI1NTVa
Fw0yNDEwMDEwODI1NTVaMEsxCzAJBgNVBAYTAk5MMRIwEAYDVQQHDAlBbXN0ZXJk
YW0xDjAMBgNVBAoMBUljaG9yMRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEi
MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDIisWpB9FSc/OIYsxznQJgioL2
aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ+VNc63blCPAI
iL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFFelbLf7JTW80C
nNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi1QbWD3G1lkTQ
cCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGoswxe5IF+yB6f
WFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0yv+K0iUBAgMB
AAGjUzBRMB0GA1UdDgQWBBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAfBgNVHSMEGDAW
gBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3
DQEBCwUAA4IBAQC1QKNwlqrvE5a7M5v151tA/JvIKNBfesIRCg/IG6tB3teSJjAq
PRsSajMcyvDtlDMkWL093B8Epsc8yt/vN1GgsCfQsMeIWRGvRYM6Qo2WoABaLTuC
TxH89YrXa9uwUnbesGwKSkpxzj8g6YuzUpLnAAV70LefBn8ZgBJktFVW/ANWwQqD
KiglduSDGX3NiLByn7nGKejg2dPw6kWpOTVtLsWtgG/5T4vw+INQzPJqy+pUvBeQ
OrV7cQyq12DJJPxboIRh1KqF8C25NAOnXVtCav985RixHCMaNaXyOXBJeEEASnXg
pdDrObCgMze03IaHQ1pj3LeMu0OvEMbsRrYW
-----END CERTIFICATE-----
$ cat key.pem
-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDIisWpB9FSc/OI
YsxznQJgioL2aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ
+VNc63blCPAIiL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFF
elbLf7JTW80CnNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi
1QbWD3G1lkTQcCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGo
swxe5IF+yB6fWFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0
yv+K0iUBAgMBAAECggEAB2lJgMOvMrLiTyoHkEY/Lj4wNNcNW8g0aQRWlmng7SdE
84mh1QEspJegaUe7eyNTsTY8TNwzET43jEFQmWCCVliMNmSHWWWF99sgmuL5W5aN
Ec+4LPnCWDR+pxAKcCEoN4yzhfLTKNzmsqCg0Ih5mKcN3ojZMLodpCfH3WczDkay
1KrJvIKIovIV8Yv8NJ4K9hDx7a+xGpHXqh5NLey9K6XNpDqbWwN97FygLLEoedhx
XDjPzBiGJuwIYQI/V2gYVXn3nPcglDFqHpZA8peX3+DwXojqDwzbNhf2CANpMgaD
5OQkGymb+FumKPh1UMEL+IfqtXWwwg2Ix7DtI6i/IQKBgQDasFuUC43F0NM61TFQ
hWuH4mRMp8B4YkhO9M+0/mitHI76cnU63EnJpMYvnp3CLRpVzs/sidEcbfC9gOma
Ui+xxtKers+RQDI4lrwgqyYIzNNpGO7ItlDYs3guTvX5Mvc5x/zIIHhcjhTudzHK
Utgho8PIXvrrF3fSHdL0QJb1tQKBgQDqwdGImGbF3lxsB8t4T/8fozIBwo2dWp4O
YgcVyDpq7Nl+J9Yy5fqTw/sIn6Fa6ZpqAzq0zPSoYJzU7l9+5CcpkkdPEulyER3D
maQaVMUv0cpnXUQSBRNDZJDdbC4eKocxxotyTuLjCVrUwYWuqYWo3aGx42VXWsWm
n4+9AwDBnQKBgBStumscUJKU9XRJtnkLtKhLsvpAnoWDnZzBr2ZI7DL6UVbDPeyL
6fpEN21HTVmQFD5q6ORP/9L1Xl888lniTZo816ujkgMFE/qf3jgkltscKx1z+xhF
jQ2AouuWEdI3jIMNMwzlbRwrXzVRVgbwoHlF1/x5ZraWKIFYyprIBL5FAoGAfXcM
12YsN0A6QPqBglGu1mfQCCTErv6JTsKRatDSd+cR7ly4HAfRvjuV5Ov7vqzu/A2x
yINplrvb1el4XEbvr0YgmmBPJ8mCENICZJg9sur6s/eis8bGntQWoGB63WB5VN76
FCOZGyIay26KVekAKFobWwlfViqLTBwnJCuAsfkCgYAgrbUKCf+BGgz4epv3T3Ps
kJD2XX1eeQr/bw1IMqY7ZLKCjg7oxM1jjp2xyujY3ZNViaJsnj7ngCIbaOSJNa3t
YS8GDLTL4efNTmPEVzgxhnJdSeYVgMLDwu66M9EYhkw+0Y1PQsNS1+6SO89ySRzP
pFbool/8ZDecmB4ZSa03aw==
-----END PRIVATE KEY-----
```

Now we need to get these into the code. We can do this by reading a file using standard C++, but for convenience, this doc will just copy and paste them into code:

```c++
auto queue = std::make_unique<MultimapQueue>();
auto &dm = queue->createManager();

std::string key = "-----BEGIN PRIVATE KEY-----\n"
                  "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDIisWpB9FSc/OI\n"
                  "YsxznQJgioL2aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ\n"
                  "+VNc63blCPAIiL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFF\n"
                  "elbLf7JTW80CnNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi\n"
                  "1QbWD3G1lkTQcCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGo\n"
                  "swxe5IF+yB6fWFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0\n"
                  "yv+K0iUBAgMBAAECggEAB2lJgMOvMrLiTyoHkEY/Lj4wNNcNW8g0aQRWlmng7SdE\n"
                  "84mh1QEspJegaUe7eyNTsTY8TNwzET43jEFQmWCCVliMNmSHWWWF99sgmuL5W5aN\n"
                  "Ec+4LPnCWDR+pxAKcCEoN4yzhfLTKNzmsqCg0Ih5mKcN3ojZMLodpCfH3WczDkay\n"
                  "1KrJvIKIovIV8Yv8NJ4K9hDx7a+xGpHXqh5NLey9K6XNpDqbWwN97FygLLEoedhx\n"
                  "XDjPzBiGJuwIYQI/V2gYVXn3nPcglDFqHpZA8peX3+DwXojqDwzbNhf2CANpMgaD\n"
                  "5OQkGymb+FumKPh1UMEL+IfqtXWwwg2Ix7DtI6i/IQKBgQDasFuUC43F0NM61TFQ\n"
                  "hWuH4mRMp8B4YkhO9M+0/mitHI76cnU63EnJpMYvnp3CLRpVzs/sidEcbfC9gOma\n"
                  "Ui+xxtKers+RQDI4lrwgqyYIzNNpGO7ItlDYs3guTvX5Mvc5x/zIIHhcjhTudzHK\n"
                  "Utgho8PIXvrrF3fSHdL0QJb1tQKBgQDqwdGImGbF3lxsB8t4T/8fozIBwo2dWp4O\n"
                  "YgcVyDpq7Nl+J9Yy5fqTw/sIn6Fa6ZpqAzq0zPSoYJzU7l9+5CcpkkdPEulyER3D\n"
                  "maQaVMUv0cpnXUQSBRNDZJDdbC4eKocxxotyTuLjCVrUwYWuqYWo3aGx42VXWsWm\n"
                  "n4+9AwDBnQKBgBStumscUJKU9XRJtnkLtKhLsvpAnoWDnZzBr2ZI7DL6UVbDPeyL\n"
                  "6fpEN21HTVmQFD5q6ORP/9L1Xl888lniTZo816ujkgMFE/qf3jgkltscKx1z+xhF\n"
                  "jQ2AouuWEdI3jIMNMwzlbRwrXzVRVgbwoHlF1/x5ZraWKIFYyprIBL5FAoGAfXcM\n"
                  "12YsN0A6QPqBglGu1mfQCCTErv6JTsKRatDSd+cR7ly4HAfRvjuV5Ov7vqzu/A2x\n"
                  "yINplrvb1el4XEbvr0YgmmBPJ8mCENICZJg9sur6s/eis8bGntQWoGB63WB5VN76\n"
                  "FCOZGyIay26KVekAKFobWwlfViqLTBwnJCuAsfkCgYAgrbUKCf+BGgz4epv3T3Ps\n"
                  "kJD2XX1eeQr/bw1IMqY7ZLKCjg7oxM1jjp2xyujY3ZNViaJsnj7ngCIbaOSJNa3t\n"
                  "YS8GDLTL4efNTmPEVzgxhnJdSeYVgMLDwu66M9EYhkw+0Y1PQsNS1+6SO89ySRzP\n"
                  "pFbool/8ZDecmB4ZSa03aw==\n"
                  "-----END PRIVATE KEY-----";

std::string cert = "-----BEGIN CERTIFICATE-----\n"
                   "MIIDdzCCAl+gAwIBAgIUQEtUTmX2HbHwOpDPgFkYx66ei/swDQYJKoZIhvcNAQEL\n"
                   "BQAwSzELMAkGA1UEBhMCTkwxEjAQBgNVBAcMCUFtc3RlcmRhbTEOMAwGA1UECgwF\n"
                   "SWNob3IxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAeFw0yMzEwMDIwODI1NTVa\n"
                   "Fw0yNDEwMDEwODI1NTVaMEsxCzAJBgNVBAYTAk5MMRIwEAYDVQQHDAlBbXN0ZXJk\n"
                   "YW0xDjAMBgNVBAoMBUljaG9yMRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEi\n"
                   "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDIisWpB9FSc/OIYsxznQJgioL2\n"
                   "aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ+VNc63blCPAI\n"
                   "iL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFFelbLf7JTW80C\n"
                   "nNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi1QbWD3G1lkTQ\n"
                   "cCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGoswxe5IF+yB6f\n"
                   "WFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0yv+K0iUBAgMB\n"
                   "AAGjUzBRMB0GA1UdDgQWBBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAfBgNVHSMEGDAW\n"
                   "gBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\n"
                   "DQEBCwUAA4IBAQC1QKNwlqrvE5a7M5v151tA/JvIKNBfesIRCg/IG6tB3teSJjAq\n"
                   "PRsSajMcyvDtlDMkWL093B8Epsc8yt/vN1GgsCfQsMeIWRGvRYM6Qo2WoABaLTuC\n"
                   "TxH89YrXa9uwUnbesGwKSkpxzj8g6YuzUpLnAAV70LefBn8ZgBJktFVW/ANWwQqD\n"
                   "KiglduSDGX3NiLByn7nGKejg2dPw6kWpOTVtLsWtgG/5T4vw+INQzPJqy+pUvBeQ\n"
                   "OrV7cQyq12DJJPxboIRh1KqF8C25NAOnXVtCav985RixHCMaNaXyOXBJeEEASnXg\n"
                   "pdDrObCgMze03IaHQ1pj3LeMu0OvEMbsRrYW\n"
                   "-----END CERTIFICATE-----";

dm.createServiceManager<AsioContextService, IAsioContextService>();
dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(443))}, {"SslKey", Ichor::make_any<std::string>(key)}, {"SslCert", Ichor::make_any<std::string>(cert)}});

queue->start(CaptureSigInt);
```

This will create a Https-only Host on 127.0.0.1:443. Be mindful that ports below 1000 usually require some sort of root or privileged user to use it. If you want to support Http as well, simply create another Host Service:

```c++
dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8000))}});
```

## Client

Connecting to a Https webserver is somewhat similar. Using the same certificate from above, we need to tell the client to trust it:

```c++
auto queue = std::make_unique<MultimapQueue>();
auto &dm = queue->createManager();

std::string cert = "-----BEGIN CERTIFICATE-----\n"
                   "MIIDdzCCAl+gAwIBAgIUQEtUTmX2HbHwOpDPgFkYx66ei/swDQYJKoZIhvcNAQEL\n"
                   "BQAwSzELMAkGA1UEBhMCTkwxEjAQBgNVBAcMCUFtc3RlcmRhbTEOMAwGA1UECgwF\n"
                   "SWNob3IxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAeFw0yMzEwMDIwODI1NTVa\n"
                   "Fw0yNDEwMDEwODI1NTVaMEsxCzAJBgNVBAYTAk5MMRIwEAYDVQQHDAlBbXN0ZXJk\n"
                   "YW0xDjAMBgNVBAoMBUljaG9yMRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEi\n"
                   "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDIisWpB9FSc/OIYsxznQJgioL2\n"
                   "aKlqf0fKzrP6IVSq1ApxlMF+hVpz340Yv5rVYizfb/zx46pTbPPJ+VNc63blCPAI\n"
                   "iL7h3NdYsMaAe+dTfdmQd2/m1kiBQFraceOw5TB7+Y9lFXfUCUFFelbLf7JTW80C\n"
                   "nNOP4xPmXCusMD8m4LL8z7e7BqKr4VFrrAFXB0EBqpmHA8HXbKAi1QbWD3G1lkTQ\n"
                   "cCrq8c7pw5oME0RNjfrevlGHATJ2a6zCAeUsIfsDGFpNHFIYqGGoswxe5IF+yB6f\n"
                   "WFWMjtbbRv93pZfViTFcOUWQuhLFsSN41bt9/XimlxM+ZtX5JDE0yv+K0iUBAgMB\n"
                   "AAGjUzBRMB0GA1UdDgQWBBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAfBgNVHSMEGDAW\n"
                   "gBRgTmWQZDeiFMXtL8tyQSdD/HRYPjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\n"
                   "DQEBCwUAA4IBAQC1QKNwlqrvE5a7M5v151tA/JvIKNBfesIRCg/IG6tB3teSJjAq\n"
                   "PRsSajMcyvDtlDMkWL093B8Epsc8yt/vN1GgsCfQsMeIWRGvRYM6Qo2WoABaLTuC\n"
                   "TxH89YrXa9uwUnbesGwKSkpxzj8g6YuzUpLnAAV70LefBn8ZgBJktFVW/ANWwQqD\n"
                   "KiglduSDGX3NiLByn7nGKejg2dPw6kWpOTVtLsWtgG/5T4vw+INQzPJqy+pUvBeQ\n"
                   "OrV7cQyq12DJJPxboIRh1KqF8C25NAOnXVtCav985RixHCMaNaXyOXBJeEEASnXg\n"
                   "pdDrObCgMze03IaHQ1pj3LeMu0OvEMbsRrYW\n"
                   "-----END CERTIFICATE-----";

dm.createServiceManager<AsioContextService, IAsioContextService>();
dm.createServiceManager<HttpConnectionService, IHttpConnectionService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(443))}, {"ConnectOverSsl", Ichor::make_any<bool>(true)}, {"RootCA", Ichor::make_any<std::string>(cert)}});

queue->start(CaptureSigInt);
```

And you should be good to go.
