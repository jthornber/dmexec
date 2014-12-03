: show-foo :foo get . ;
: set-foo :foo set ;

45 set-foo
namespace namespace-push
55 set-foo
show-foo
namespace-pop
show-foo

[ 65 set-foo show-foo ] with-namespace
show-foo