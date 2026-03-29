/**
 * Author: Jack Robbins
 * The visibility header centralizes ways of determining whether a function/varialbe
 * is public or private
*/

#ifndef VISIBILITY_H
#define VISIBILITY_H

/**
 * Is the given quantity private or public?
 */
typedef enum {
	VISIBILITY_TYPE_PRIVATE,
	VISIBILITY_TYPE_PUBLIC,
} visibilty_type_t;

#endif /* VISIBILITY_H */
