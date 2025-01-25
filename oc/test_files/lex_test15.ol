func: main(u_int32 argc, char** argv) -> u_int32 {
   
    while argc < 5 do {
        declare u_int32 i;
        asn i := 3;
    }

    declare u_int32* address_array := malloc(32 * sizeof(u_int32));
    if address_array == null do {
        ret 1;  
    }

    for i in 0..32 do {
        asn address_array[i] := 0;
    }

    declare u_int32 j;

    for j in 0..34 do {
        asn j := j - 3;
    }

    free(address_array);

    ret j;
}