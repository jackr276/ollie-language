/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/
func:static main(u_int32 arg_count, str arg_vector) -> u_int32{
	if(arg_count == 0) then {
		asn arg_count := -1;
		asn arg_vector := "hello";
	} else {
		let float32 a := .23;
		let float32 b := 2.322;
		let float32 c := a + b;
	}

	ret (u_int32)c >> 3;
}
