/**
 * Author: Jack Robbins
 * Test our ability to do math with a multifile namespace
 */

$module math;

namespace calculator {
	namespace add {
		pub fn calculate(x:mut i32*, y:i32) -> void {
			*x += y;
		}
	}

	namespace subtract {
		pub fn calculate(x:mut i32*, y:i32) -> void {
			*x -= y;
		}
	}
}
