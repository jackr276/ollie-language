/**
* Sample program
*/

func main() -> u_int32 {
	u_int8 i = 0;
	ref u_int8 i_ptr = memaddr(i);

	u_int8 j /*also single line comment */= 1;
	ref u_int8 j_ptr = memaddr(j);

	ret deref(i) + deref(j)
}
