// Self-signed cert for captive portal HTTPS
#ifndef PORTAL_CERTS_H
#define PORTAL_CERTS_H

static const unsigned char portal_cert_pem[] =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDEzCCAfugAwIBAgIUB+zNdZ1zOsr2I+79qnyuE0onzrkwDQYJKoZIhvcNAQEL\n"
  "BQAwGTEXMBUGA1UEAwwOY2FwdGl2ZS5wb3J0YWwwHhcNMjYwMjEyMDI0MTE2WhcN\n"
  "MzYwMjEwMDI0MTE2WjAZMRcwFQYDVQQDDA5jYXB0aXZlLnBvcnRhbDCCASIwDQYJ\n"
  "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ52q0C+BLqE8NO8WbGMlIs7PTt5jY4R\n"
  "fxyDwuu+vUYdkwj9FKr541QZgUg/ek3WO9MWnj+VTCcjsFTy3qJQU6DYUQxi3V/o\n"
  "Sui6bov50JUx5/X+GwQnFD3QRjzTc/eAPtx7QxPJw3v3eglKrXPLZqcLoelCyjLm\n"
  "yLmGX1RzzpUQUFwgLW8B+9/lQTb5pcWF7qHvZ0mH1mvXpDD3DJqZOfmQQqDd9g9R\n"
  "i6Dd48VscIZYMhxfJKKyMjsRMKo0GbMqTWGz/sHUF/jvNj4WGI0ncogQJsP/dSSv\n"
  "jEjcVejeoGt0/397wPF4e+WgKpgATHLrSbmE1WVExtLCNoxsQevVfykCAwEAAaNT\n"
  "MFEwHQYDVR0OBBYEFICyf4GEXWx4iHk9Pz/EkQX6FqwNMB8GA1UdIwQYMBaAFICy\n"
  "f4GEXWx4iHk9Pz/EkQX6FqwNMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n"
  "BQADggEBAAJ6nYQQ+Kt7BpU9m2zNht+GW3GWmHSmpDQka/qr2xSYQs7vuft1RF6n\n"
  "yM/l63S4roQf0D6dJ5hit1viRyD5jpLsD5u+usXoKcpxsfd1tVlcLXAz3qIao0wj\n"
  "j9Z69aBm8Dnigt0g4Zi0RT4o4L4N117ETWZeHlFm4gzyvEZxpnJ6xt59fAw5iLTv\n"
  "rG0lC+/MAPTpmdazO+o8nD3iknIK8pzDCkB4BJCYxvU7xVLqLxPqk8KjWrJS+2Xi\n"
  "Aqocmp1O708L13dU0aJd5i11xlnRe+OengdezSg2r9r4Nh1viZkvNs9FSlVy189Y\n"
  "JuLn6S1cTfvYcO5voqGnjHpBAwMEc6U=\n"
  "-----END CERTIFICATE-----\n";
static const unsigned int portal_cert_pem_len = sizeof(portal_cert_pem);

static const unsigned char portal_key_pem[] =
  "-----BEGIN PRIVATE KEY-----\n"
  "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCedqtAvgS6hPDT\n"
  "vFmxjJSLOz07eY2OEX8cg8Lrvr1GHZMI/RSq+eNUGYFIP3pN1jvTFp4/lUwnI7BU\n"
  "8t6iUFOg2FEMYt1f6Eroum6L+dCVMef1/hsEJxQ90EY803P3gD7ce0MTycN793oJ\n"
  "Sq1zy2anC6HpQsoy5si5hl9Uc86VEFBcIC1vAfvf5UE2+aXFhe6h72dJh9Zr16Qw\n"
  "9wyamTn5kEKg3fYPUYug3ePFbHCGWDIcXySisjI7ETCqNBmzKk1hs/7B1Bf47zY+\n"
  "FhiNJ3KIECbD/3Ukr4xI3FXo3qBrdP9/e8DxeHvloCqYAExy60m5hNVlRMbSwjaM\n"
  "bEHr1X8pAgMBAAECggEAElCH2Xs8w6vyw54MxcsILhFiMdoKJMXb6ZlG8YVBjfWE\n"
  "FXFerqYkAofSG94OTim5D1wZXSLtwsZKrn7FgYAEMl5d+67/hAKuibhmZ3Ivi9bR\n"
  "RuqgB7pWihBk1cngqNXfLg9nAMX8fHrtVV6Wrn/vNNZQ18ta4EgxIdQod3PNCAhE\n"
  "WMzzniECdk5g7c9ZHoPCwdhkdvYfo5pxre5vCElb5ietDlgf4Z4X8YfQ1hEOiSZh\n"
  "8g69JfsQS++sLVQWb2tTkNoqeuLR6volU2QQFVbZhOTdfeEUI/mSJMHcLU+wyd6I\n"
  "pgF7XKemaN8ERs7l42mLb32+W9vI3QC7cHcvCgN3IwKBgQDfvN1IHerMtitb8ojd\n"
  "0v242v1Z3LxuG3LR2utchhq3oP21BCiYLQUL8OoQygP296JssvW8KBXQP4tMx1pR\n"
  "jET5CwzKnYLQKsc1IHq7SLxhOpmV1RHRCNmqJ6s6R90cUnQ3br7JowyCRGKurxca\n"
  "WmDgvJH6UAC6tBBSIJBuSZerDwKBgQC1UD/SrLYqRMCypigvqPVvTA1msHz6Cqqj\n"
  "T5p3kCEnZWm8ROEX3TI8aLLErDlS3n330qsNkqaUizKb6gHBaLmHO02T/XlSBN9Y\n"
  "nc4UGUcg7P3lSKzq/1ZCcRbZconnI7kP9+0h0bD4g4jZAuDkSITFY7icVZg/IK+L\n"
  "mDJu02ISRwKBgQCtBieyb8B3E6Iwdz21Hkgsvt4SsoveMsf21nARm3vp6kyJwm+K\n"
  "T7aJZ4P7+95ZDWTE3xj+q5lC9QFLHRgR3IIyfoMj5e+WAJ8RzM5dK4DTSDgiESa4\n"
  "GzoJRqg6Z/zsvC839yMAWGGCFFSkdq9NJ/unkOzrQRdvS3UbridDwxHRrQKBgQCA\n"
  "XHDiuG063XPCxkk6/JXNln0CTz4bUwPdHFUSAlLGblB9/hGRcILJK6IsBHWMyoFJ\n"
  "urZopIMUNLu+j9twTrDSIVTZyaBjRZnNReMIbaDimYViFqJ8uAocfa2cgi9JGQxC\n"
  "VtZlk+uC9LQkVppaY1DD4cZnv11Ki9xbqoNB3JrYrwKBgA/jG2t2rHF7IAdhlau8\n"
  "TX/RpAaxY1pgFawKBJKq2kL4Du1POdTIj9ekaDWfwjonV6he0EEElPM/FkFKEVyX\n"
  "38R1Z1xPrhGE6CbuHxP+abA0bBCkESFsatfCo1AwNnKOF0zct703MtbUodHcE/nO\n"
  "LjB3qqzd2UN8KdKDKeGk5KnS\n"
  "-----END PRIVATE KEY-----\n";
static const unsigned int portal_key_pem_len = sizeof(portal_key_pem);

#endif
