/**
* Sample program
*/

declare u_int8 j;

func main() -> u_int32 {
	//Single line comment
	let u_int8 i := 0;
    let u_int8* i_ptr := &i; //Another comment

	let u_int8 j /*also single line comment */:= 1;
	let u_int8* j_ptr := &j;

	ret *i + *j;
}
