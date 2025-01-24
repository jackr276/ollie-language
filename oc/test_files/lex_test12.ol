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
	let u_int8 b := 9;
	//Should fail
	while(b >= 9 && b <= "hi"){
		b--;
	}

	ret;
}
