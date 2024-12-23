/**
* Sample program
*/

u_int32 main(){
	u_int8 i = 0;
	ref u_int8 i_ptr = memaddr(i);

	u_int8 j = 1;
	ref u_int8 j_ptr = memaddr(j);

	ret deref(i) + deref(j)
}
