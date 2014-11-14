"foo" "lskjdfl" dm-create

{ { 512 "linear" "/dev/vdc 0" }
  { 1024 "linear" "/dev/vdc 2048" }
} "foo" dm-load
"foo" dm-resume
"foo" dm-remove
