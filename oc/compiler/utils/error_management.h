/**
 * Author: Jack Robbins
 * This header file holds everything that we need to do about errors
*/

#ifndef ERROR_MANAGEMENT_H
#define ERROR_MANAGEMENT_H

/**
 * What type of message do we have
 */
typedef enum {
	MESSAGE_TYPE_WARNING,
	MESSAGE_TYPE_ERROR,
	MESSAGE_TYPE_INFO,
	MESSAGE_TYPE_DEBUG,
} error_message_type_t;

#endif /* ERROR_MANAGEMENT_H */
