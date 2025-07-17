/**
* Author: Jack Robbins
* Testing scenarios with illegal nested defers
*/

fn main() -> i32 {
	let mut x:i32 := 0;
	let mut y:i32 := 0;
	let mut z:i32 := 0;

	while ( x <= 3) do {
		x++;
		defer {
			y--;
		};
	}


	//Do while loop
	do {
		x += y;
		defer {
			x -= 43;
		};

	} while ( x < 3);

	//For loop
	for(let mut i:i32 := 0; i < 89; i++) do{
		x++;

		defer {
			z += 6;
		};
	}

	//Double nest
	if (x == 0) then {
		if ( y == 0) then {
			defer {
				x--;
			};
		}
	}

	
	ret x;
}
