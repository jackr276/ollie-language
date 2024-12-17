func main() -> s_int32{
	s_int32 i = 3;
	s_int32 ref iptr = memaddr(i);
	s_int32 j = 4;
	s_int32 ref iptr = memaddr(j); 
	s_int32 l = deref(i) + deref(j);

	if(l == 7){
		return 0;
	} else {
		return l;
	}
}
