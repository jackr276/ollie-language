/**
* Has a bad declared word deep in if else
*/
alias char* as str;

define construct con{
	i8 x;
	i8 y;
	char*[32] b;
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
