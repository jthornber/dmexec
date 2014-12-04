: square dup * ;

: neg 0 swap - ;

: drop 1 ndrop ;
: 2drop 2 ndrop ;
: 3drop 3 ndrop ;

: nip 1 nnip ;
: 2nip 2 nnip ;

: 2dip swap [ dip ] dip ;
: 3dip swap [ 2dip ] dip ;
: 4dip swap [ 3dip ] dip ;

: keep over [ call ] dip ;
: 2keep [ 2dup ] dip 2dip ;
: 3keep [ 3dup ] dip 3dip ;

: 2curry curry curry ;

: swapd [ swap ] dip ;
: with swapd [ swapd call ] 2curry ;

: bi [ keep ] dip call ;
: 2bi [ 2keep ] dip call ;
: 3bi [ 3keep ] dip call ;

: tri [ [ keep ] dip keep ] dip call ;
: 2tri [ [ 2keep ] dip 2keep ] dip call ;
: 3tri [ [ 3keep ] dip 3keep ] dip call ;

: cleave [ call ] with each ;
: 2cleave [ 2keep ] each 2drop ;
: 3cleave [ 3keep ] each 3drop ;
: 4cleave [ 4keep ] each 4drop ;

: bi* [ dip ] dip call ;
: 2bi* [ 2dip ] dip call ;
: tri* [ [ 2dip ] dip dip ] dip call ;
: 2tri* [ 4dip ] 2dip 2bi* ;

: bi@ dup bi* ;
: 2bi@ dup 2bi* ;
: tri@ dup dup tri* ;
: 2tri@ dip dip 2tri* ;

: dupd [ dup ] dip ;
: or dupd ? ;
: and over ? ;

: both? bi@ and ;
: either? bi@ or ;

: if ? call ;
: when swap [ call ] [ drop ] if ;
: unless swap [ drop ] [ call ] if ;
: if* pick [ drop call ] [ 2nip call ] if ;
: when* over [ call ] [ 2drop ] if ;
: unless* over [ drop ] [ nip call ] if ;

: ?if pick [ drop [ drop ] 2dip call ] [ 2nip call ] if ;

: loop [ call ] keep [ loop ] curry when ;

: with-namespace namespace namespace-push call namespace-pop ;


: test-case call eq [ "failed test" error ] unless ;