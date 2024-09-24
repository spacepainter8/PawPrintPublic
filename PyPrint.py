import mysql.connector
import serial
import time

#### SQL COMMUNICATION
conn = mysql.connector.connect(host="localhost",
                             user="root",
                             password="123",
                             database="arduinodb")
cursor = conn.cursor()

#### SERIAL COMMUNICATION

serialPort = serial.Serial(port='COM9', baudrate=115200, timeout=.1)
time.sleep(2)


def main():
    print("start")
    username = ""
    password = ""
    isAdmin = 0
    usernameFromList = ""


    while True:
        data = serialPort.readline().decode('utf-8')
        if data == "":
            continue

        option = data[0]
        if option == '0':
        # checking whether a username is taken

            username = data[1:]
            query = "SELECT * FROM users where username = %s"
            cursor.execute(query, (username, ))
            cursor.fetchall()
            result = cursor.rowcount
            if result == 0:
                serialPort.write(bytes("0", 'utf-8'))
            else:
                serialPort.write(bytes("1", 'utf-8'))
        elif option == '1':
        # inserting a user into the system

            (password, fingerprintID) = data[1:].split('!')
            query = "INSERT INTO users VALUES(%s, %s, 0)"

            if password == "":
                password = None

            cursor.execute(query, (username, password))
            conn.commit()

            if cursor.rowcount == 1:
                if fingerprintID != "-1":
                    query = "UPDATE fingerprints SET username=%s WHERE fingerprintID = %s"
                    cursor.execute(query, (username, fingerprintID))
                    conn.commit()
                    if cursor.rowcount == 0:
                        serialPort.write(bytes("1", 'utf-8'))
                        continue
                serialPort.write(bytes("0", 'utf-8'))
            else:
                serialPort.write(bytes("1", 'utf-8'))
        elif option == '2':
        # logging in

            (username, password) = data[1:].split('!')


            query = "SELECT * FROM users WHERE username = %s"
            cursor.execute(query, (username, ))
            result = cursor.fetchone()


            # the user doesn't exist
            if cursor.rowcount == 0:
                serialPort.write(bytes("2", 'utf-8'))
                continue;

            # the password doesn't match
            if result[1] != password:
                serialPort.write(bytes("3", 'utf-8'))
                continue

            # all good, send whether user is admin or not
            usernameFromList = ""
            isAdmin = result[2]

            if result[2] == 0:
                serialPort.write(bytes("0", 'utf-8'))
                continue

            if result[2] == 1:
                serialPort.write(bytes("1", 'utf-8'))
                continue

        elif option == '3':
        # select the next available fingerprint ID

            query = "SELECT MIN(fingerprintID) FROM fingerprints WHERE username is NULL"
            cursor.execute(query)
            data = cursor.fetchone()

            x = str(data[0]) + "!"
            serialPort.write(bytes(x, 'utf-8'))

        elif option == '4':
            # arduino is asking for username after user logged in via fingerprint ID

            fingerprintID = data[1:]
            query = "SELECT f.username, u.isAdmin FROM fingerprints f join users u on (f.username=u.username) WHERE fingerprintID = %s"
            cursor.execute(query, (fingerprintID, ))
            data = cursor.fetchone()
            if cursor.rowcount == 0:
                serialPort.write(bytes("$", 'utf-8'))
                continue
            username = data[0] + "!"
            serialPort.write(bytes(username, 'utf-8'))
            serialPort.write(repr(data[1]).encode('utf-8'))
            username = data[0]
            usernameFromList = ""
            isAdmin = data[1]

        elif option == '5':
            # arduino is asking whether user exists and whether user has fingerprint in the system

            username = data[1:]
            query = "SELECT username FROM users WHERE username=%s"
            cursor.execute(query, (username, ))
            cursor.fetchall()
            if cursor.rowcount == 0:
                # user with given username doesn't exist
                serialPort.write(bytes("1", 'utf-8'))
                continue
            else :
                # check for fingerprint in the system
                query = "SELECT * FROM fingerprints WHERE username=%s"
                cursor.execute(query, (username, ))
                cursor.fetchall()
                if cursor.rowcount == 0:
                    # user with given username has no fingerprints in the system
                    serialPort.write(bytes("2", 'utf-8'))
                    continue
                else :
                    # all good
                    serialPort.write(bytes("0", 'utf-8'))

        elif option == '6':
            # check whether fingerprintID matches the user
            fingerprintID = data[1:]
            query = "SELECT * FROM fingerprints WHERE fingerprintID=%s and username = %s"
            cursor.execute(query, (fingerprintID, username))
            cursor.fetchall()
            if cursor.rowcount == 0:
                # authorization failed
                serialPort.write(bytes("1", 'utf-8'))
            else:
                # authorization ok
                serialPort.write(bytes("0", 'utf-8'))

        elif option == '7':
            # update password
            password = data[1:]
            query = "UPDATE users SET password = %s WHERE username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (password, username))
            else:
                cursor.execute(query, (password, usernameFromList))
            conn.commit()
            if cursor.rowcount == 0:
                serialPort.write(bytes("1", 'utf-8'))
            else:
                serialPort.write(bytes("0", 'utf-8'))

        elif option == '8':
            # get previous user from list
            usernameFromList = data[1:]
            query = "SELECT username FROM users WHERE username < %s ORDER BY username DESC LIMIT 1"
            cursor.execute(query, (usernameFromList, ))
            data = cursor.fetchone()
            if cursor.rowcount == 0:
                # there are no previous users, get the last one
                query = "SELECT username FROM users ORDER BY username DESC LIMIT 1"
                cursor.execute(query)
                data = cursor.fetchone()
                usernameFromList = data[0]
                serialPort.write(bytes(data[0], 'utf-8'))
            else:
                usernameFromList = data[0]
                serialPort.write(bytes(data[0], 'utf-8'))
            serialPort.write(bytes("!", 'utf-8'))

        elif option == '9':
            # get next user from list
            usernameFromList = data[1:]
            query = "SELECT username FROM users WHERE username > %s ORDER BY username LIMIT 1"
            cursor.execute(query, (usernameFromList, ))
            data = cursor.fetchone()
            if cursor.rowcount == 0:
                # there are no next users, get the first one
                query = "SELECT username FROM users ORDER BY username LIMIT 1"
                cursor.execute(query)
                data = cursor.fetchone()
                usernameFromList = data[0]
                serialPort.write(bytes(data[0], 'utf-8'))
            else:
                usernameFromList = data[0]
                serialPort.write(bytes(data[0], 'utf-8'))
            serialPort.write(bytes("!", 'utf-8'))

        elif option == 'A':
            # first check whether username is unique
            newUsername = data[1:]
            query = "SELECT * FROM users where username = %s"
            cursor.execute(query, (newUsername,))
            cursor.fetchall()
            result = cursor.rowcount
            if result == 0:
                # serialPort.write(bytes("0", 'utf-8'))

                query = "UPDATE users SET username = %s WHERE username = %s"
                if isAdmin == 0 or usernameFromList == "" :
                    cursor.execute(query, (newUsername, username))
                else:
                    cursor.execute(query, (newUsername, usernameFromList))
                conn.commit()

                if isAdmin == 1:
                    if usernameFromList == username or usernameFromList == "":
                        username = newUsername
                    usernameFromList = newUsername
                else:
                    username = newUsername
                serialPort.write(bytes("0", 'utf-8'))
            else:
                serialPort.write(bytes("1", 'utf-8'))

        elif option == 'B':
            # check whether user has password in the system

            query = "SELECT password FROM users where username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username, ))
            else:
                cursor.execute(query, (usernameFromList, ))
            data = cursor.fetchone()
            if data[0] is None:
                # user doesn't have password in system
                serialPort.write(bytes("0", 'utf-8'))
            else:
                serialPort.write(bytes("1", 'utf-8'))

        elif option == 'C':
            # check whether user has fingerprint in the system

            query = "SELECT * FROM fingerprints WHERE username=%s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username,))
            else:
                cursor.execute(query, (usernameFromList, ))
            cursor.fetchall()
            if cursor.rowcount == 0:
                # user with given username has no fingerprints in the system
                serialPort.write(bytes("1", 'utf-8'))
                continue
            else:
                # all good
                serialPort.write(bytes("0", 'utf-8'))

        elif option == 'D':
            # delete password for user
            query = "UPDATE users SET password = NULL WHERE username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username, ))
            else:
                cursor.execute(query, (usernameFromList, ))
            conn.commit()
            serialPort.write(bytes("0", 'utf-8'))

        elif option == 'E':
            # insert fingerprint into the database
            fingerprintID = data[1:]
            query = "UPDATE fingerprints SET username = %s where fingerprintID = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username, fingerprintID))
            else:
                cursor.execute(query, (usernameFromList, fingerprintID))
            conn.commit()
            serialPort.write(bytes("0", 'utf-8'))

        elif option == 'F':
            # used in deleting fingerprint option

            fingerprintID = data[1:]
            # check whether fingerprint belongs to the logged in user
            query = "SELECT * FROM fingerprints WHERE fingerprintID = %s and username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (fingerprintID, username))
            else:
                cursor.execute(query, (fingerprintID, usernameFromList))
            cursor.fetchone()
            result = cursor.rowcount
            # if isAdmin == 1:
            #     result = 1
            if result == 0:
                # the fingerprint doesn't belong to this user
                serialPort.write(bytes("1", 'utf-8'))
            else:
                # check whether user has alternative logging in methods
                query = "SELECT password FROM users WHERE username = %s"
                if isAdmin == 0 or usernameFromList == "":
                    cursor.execute(query, (username, ))
                else:
                    cursor.execute(query, (usernameFromList, ))
                data = cursor.fetchone()
                password = data[0]
                if password is None:
                    # check user has other fingerprints
                    query = "SELECT * FROM fingerprints WHERE username = %s"
                    if isAdmin == 0 or usernameFromList == "":
                        cursor.execute(query, (username, ))
                    else:
                        cursor.execute(query, (usernameFromList, ))
                    data = cursor.fetchall()
                    result = cursor.rowcount
                    if result == 1:
                        # user has no alternative logging in method
                        serialPort.write(bytes("2", 'utf-8'))
                        continue


                # all good, update the db
                query = "UPDATE fingerprints SET username = NULL WHERE fingerprintID = %s"
                cursor.execute(query, (fingerprintID, ))
                conn.commit()
                serialPort.write(bytes("0", 'utf-8'))

        elif option == 'G':
            # deleting the user


            # get all the fingerprint IDs
            query = "SELECT fingerprintID FROM fingerprints WHERE username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username, ))
            else:
                cursor.execute(query, (usernameFromList, ))
            fingerprints = cursor.fetchall()


            # delete the user and update fingerprint table
            query = "DELETE FROM users WHERE username = %s"
            if isAdmin == 0 or usernameFromList == "":
                cursor.execute(query, (username,))
            else:
                cursor.execute(query, (usernameFromList,))
            conn.commit()


            # let arduino know how many fingerprints it needs to remove
            x = str(len(fingerprints)) + "!"
            serialPort.write(bytes(x, 'utf-8'))

            # send over all the fingerprint IDs
            for idArr in fingerprints:
                fId = idArr[0]
                x = str(fId) + "!"
                serialPort.write(bytes(x, 'utf-8'))


            # let arduino know the operation is done
            serialPort.write(bytes("$", 'utf-8'))

            # if the logged in user is an admin, let arduino know whether logging out is needed
            if isAdmin == 1:
                if usernameFromList == "" or (username == usernameFromList):
                    # logging out is needed
                    serialPort.write(bytes("0", 'utf-8'))
                else:
                    serialPort.write(bytes("1", 'utf-8'))
                continue

        elif option == 'H':
            query = "UPDATE users SET isAdmin = 1 - isAdmin WHERE username = %s "
            if usernameFromList == "":
                cursor.execute(query, (username,))
            else:
                cursor.execute(query, (usernameFromList,))
            conn.commit()

            # check whether user changed admin status for themselves
            if usernameFromList == "" or (username == usernameFromList):
                # for himself
                serialPort.write(bytes("0", 'utf-8'))
                isAdmin = 1 - isAdmin
            else:
                serialPort.write(bytes("1", 'utf-8'))

        elif option == 'I':
            # check whether fingerprintID corresponds to an admin user
            fingerprint = data[1:]
            query = "SELECT isAdmin FROM fingerprints f JOIN users u ON (f.username=u.username) WHERE fingerprintID = %s"
            cursor.execute(query, (fingerprint, ))
            data = cursor.fetchone()
            if data[0] == 0:
                serialPort.write(bytes("0", 'utf-8'))
            else:
                serialPort.write(bytes("1", 'utf-8'))


        else :
            # this is for testing purposes
            print(data)

main()


