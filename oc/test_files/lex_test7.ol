/**
* Has a bad declared word deep in if else
*/

alias char* as str;

define struct con{
	x:i8;
	y:i8;
	b:char*[32];
};

pub fn main(argc:i32, argv:char**) -> i32
{
	if(argv) {
		let i:str = "hi";
	} else if(argc >= 2) {
		if(argc == 3) {
			let i:str = "hi";
		}
	}

	ret 23;
}
