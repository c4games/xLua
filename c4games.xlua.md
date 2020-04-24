# The c4games fork of xlua.

* sync latest from tencent.
* Integrate libs:
  + lua-cjson
  + lua-protobuf
  + yasio
  + c-ares, a nonblocking dns queries used by yasio
  + zlib, used by fsni

* fsni, the native file io with follow features:
  + read file apk
  + support security file io with aes-cfb.
  + can load lua_buffer with native file data IntPtr directly. 

* Compatiable lua byte code is enabled by default.
