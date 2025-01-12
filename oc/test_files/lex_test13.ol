**
* floating point calculations
*/

func:float_calculation() -> float32 {
    let float32 pi := 3.14;
    let float32 radius := 5.0;
    let float32 area := pi * radius * radius;
    ret area;
}