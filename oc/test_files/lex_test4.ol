/**
* Here we will test bad chars in idents
*/

func main() -> u_int32{
	u_int32 `a = 23;
	u_int32 ,b = 32;

	ret `a + ,b

}

