/**
* Has a bad declared word deep in if else
*/
alias char* as str;

define construct con{
	s_int8 x;
	s_int8 y;
	char*[32] b;
};

func main(s_int32 argc, char** argv) -> s_int32
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
