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

func my_func(char** argv) -> void{
	let char* a := argv;
}

func:static main(char** argv, s_int8 argc) -> void{
	let my_enum_type a := TYPE_NORMAL;
	let u_int32 a := 32;
	let u_int8 b := 9 * 7 + 3 * a;
	//Should fail
	while(b >= 9 && b <= "hi"){
		b--;
	}

	ret;
}
