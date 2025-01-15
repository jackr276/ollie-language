/**
* Attempt at using an undefined enumerated type
*/

/**
* An enum class
*/
define enumerated type_enum{
	TYPE_NORMAL,
	TYPE_STRONG,
	TYPE_STATIC,
	TYPE_DEFINED,
	TYPE_CHAR
};

func:static main(char* argv, s_int8 argc) -> void{
	//Should fail
	let u_int32 TYPE_NORMAL := 1;
	ret;
}
