/*
*Mongo database parameters
*
*dbName - Mongo database name
*colName (topics - topicID(int)) - Mongo collection of users (username, password, superuser, topics)
*topicLoc - Mongo collection of topics (_id, topics)
*topicID - mongo fieldname for topicSet ID
*superUser - mongo fieldname for superuser flag (true/false) 
*/

#define dbName "mqGate"
#define colName "users"
#define passLoc "password"
#define topicLoc "topics"
#define topicID "_id"
#define superUser "superuser"
