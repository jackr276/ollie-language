/**
* Has a bad declared word deep in if else
*/
alias char* as str;

define construct con{
	i8 x;
	i8 y;
	char*[32] b;
};

func main(i32 argc, char** argv) -> i32
{
	if(argv) then {
		let str i := "hi";
	} else if(argc >= 2) then {
		if(argc == 3) then {
			let str i := "hi";
		}
	}

	ret 23;
}
