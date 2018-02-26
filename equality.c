#include "equality.h"
#include "error.h"
#include "mm.h"
#include "string_type.h"

//----------------------------------------------------------------

bool equalp(Value lhs, Value rhs)
{
	ObjectType lt = get_type(lhs);
	ObjectType rt = get_type(rhs);

	if (lt != rt)
		return false;

	switch (lt) {
	case FIXNUM:
		return lhs.i == rhs.i;

	case STRING:
	case SYMBOL:
		return !string_cmp(lhs.ptr, rhs.ptr);

	default:
		error("equality not implemented for this type yet");
	}

	return false;
}

//----------------------------------------------------------------

