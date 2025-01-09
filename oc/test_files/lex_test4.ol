/**
* Here we will test bad chars in idents
*/

func:static main() -> u_int32 {
	declare u_int32 a;
	let u_int32 `a = 23;
	let u_int32 ,b = 32;

	ret `a + ,b

}
