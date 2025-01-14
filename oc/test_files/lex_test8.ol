/**
* Bad parens in here
*/
func:static main(s_int32 argc, char** argv) -> u_int32 {
	let u_int32 i := 0;
	let u_int32 j := 0;
	let u_int32 k := 0;
	let u_int32 l := 0;

	if(i == 0) then {
		asn i := 2;
	} else {
		if(i == 3 + 1 - 2 then {
			asn i := 3;
		}
	}
}
