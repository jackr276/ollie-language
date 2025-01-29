
define construct my_struct {
	u_int32 a;
	s_int32 b;
	float32 c;
} as my_struct;


func test_func(u_int32 i) -> void{
	asn i := 32;
}


func main(s_int32 argc, char** argv) -> s_int32{
	//Allocate a struct
	declare my_struct my_structure;

	asn my_structure:a := 2;
	asn my_structure:b := 3;
	asn my_structure:c := 32.2;

	

}
