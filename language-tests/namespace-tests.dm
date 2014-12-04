: get-foo :foo get ;
: set-foo :foo set ;
: new-namespace namespace namespace-push ;

{ 45 55 45 }
[ 45 set-foo get-foo
  new-namespace
  55 set-foo
  get-foo
  namespace-pop
  get-foo
  3 narray ] test-case
