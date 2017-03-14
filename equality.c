#include "equality.h"
#include "mm.h"

//----------------------------------------------------------------

bool equalp(Value lhs, Value rhs)
{
	// FIXME: fixnums only atm
	if (get_type(lhs) != FIXNUM || get_type(rhs) != FIXNUM)
		return false;

	return lhs.i == rhs.i;
}

//----------------------------------------------------------------

