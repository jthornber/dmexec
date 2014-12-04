: test-cycle
  [ "a_random_uuid" dm-create ]

  swap [ dm-load ] curry

  [ dm-resume ]
  [ dm-table . ]
  [ dm-status . ]
  [ dm-remove ]

  6 narray "my-dev" swap cleave
;


{ { 512 "linear" "/dev/vdc 0" }
  { 1024 "linear" "/dev/vdc 2048" }
} test-cycle
