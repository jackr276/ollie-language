/**
* Author: Jack Robbins
* Test our ability to call to functions that are in a higher namespace
*/


namespace parent
{
	//Predeclare so they're recognized
	declare fn my_fn1() -> i32;
	declare fn my_fn2() -> i32;

	namespace child
	{
		/**
		 * This should work. Just like lexical scoping, we
		 * should be able to see things in the namespaces above us
		 */
		pub fn add_numbers_dummy() -> i32 {
			ret @my_fn1() + @my_fn2();
		}
	}

	fn my_fn1() -> i32 {
		ret 5;
	}

	fn my_fn2() -> i32 {
		ret 8;
	}
}


//This should return 13 when all is said and done
pub fn main() -> i32 {
	ret @parent::child::add_numbers_dummy();
}
