/**
* Author: Jack Robbins
* Very basic physics 1 projectile motion equation implementation, just to demonstrate
* the SSE system's functionality
*/

$macro ACC_DUE_TO_GRAVITY 9.81 $endmacro

fn get_horizontal_position(velocity_x:f64, time:f64) -> f64 {
	ret time * velocity_x;
}


fn get_vertical_position(velocity_y:f64, time:f64) -> f64 {
	let result:f64 = velocity_y * time - 0.5D * ACC_DUE_TO_GRAVITY * time * time;

	ret result;
}


pub fn main() -> i32 {
	let velocity_x:f64 = 0.422D;
	let velocity_y:f64 = 1.512D;
	let time:f64 = 5;

	@get_horizontal_position(velocity_x, time);
	@get_vertical_position(velocity_y, time);

	ret 0;
}
