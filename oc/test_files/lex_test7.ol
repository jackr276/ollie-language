/**
* Has a bad declared word deep in if else
*/
alias char* as str;

define construct con{
	x:i8;
	y:i8;
	b:char*[32];
};

fn main(argc:u32, argv:char**) -> i32
{
	if(argv) then {
		let i:str := "hi";
	} else if(argc >= 2) then {
		if(argc == 3) then {
			let i:str := "hi";
		}
	}

	ret 23;
}
