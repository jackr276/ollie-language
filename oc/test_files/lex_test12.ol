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

fn my_func(argv:char**) -> void{
	let a:char* := argv;
}

fn:static main(argv:char**, argc:i8) -> i32{
	let a:my_enum_type := TYPE_NORMAL;
	let a:u32 := 32;
	let mut b:u8 := 9 * 7 + 3 * a;
	//Should fail
	while(b >= 9 && b <= "hi"){
		b--;
	}

	ret;
}
