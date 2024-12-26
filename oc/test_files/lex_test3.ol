/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/
func main(u_int32 arg_count, str arg_vector) -> u_int32{
	if(arg_count == 0) then {
		ret -1;
	} else {
		float32 a = .23;
		float32 b = 2.322;
		float32 c = a + b;
	}

	ret (u_int32)c >> 3;
}
