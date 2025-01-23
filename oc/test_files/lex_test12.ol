/**
* Attempt at using an undefined enumerated type
*/

/**
* An enum class
*/
define enum type_enum{
	TYPE_NORMAL,
	TYPE_STRONG,
	TYPE_STATIC,
	TYPE_DEFINED,
	TYPE_CHAR
} as my_enum_type;

func:static main(char** argv, s_int8 argc) -> void{
	let my_enum_type a := TYPE_NORMAL;
	//Should fail
	let u_int32 TYPE_NORMAL := 1;
	ret;
}
