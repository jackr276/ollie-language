/**
* Has a bad declared word deep in if else
*/
func main(s_int32 argc, char** argv) -> str{
	if(argc == 1) then {
		let str i := "hi";
	} else if(argc >= 2) then {
		if(argc == 3) then {
			let str i := "hi";
		}
	}

	ret "hi";
}
