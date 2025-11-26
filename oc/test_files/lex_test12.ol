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
	let a:char** = argv;
}

pub fn main(argc:i32, argv:char**) -> i32{
	let a:my_enum_type = TYPE_STRONG;
	let b:mut u32 = 9 * 7 + 3 + a;
	//Should fail
	while(b >= 9 && b <= 32) {
		b--;
	}
	
	if(a == TYPE_STRONG){
		b = 32;
	}

	ret 32;
}
