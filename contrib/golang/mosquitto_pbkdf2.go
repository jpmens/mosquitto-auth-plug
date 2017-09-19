package main

import (
	"crypto/sha256"
	"encoding/base64"
	"golang.org/x/crypto/pbkdf2"
	"math/rand"
	"strconv"
	"strings"
	"time"
	"fmt"
)

var (
	separator  = "$"
	tag        = "PBKDF2"
	algorithm  = "sha256"
	iterations = 901
	keyLen     = 48
	saltLen    = 12
)

func CratePassword(password string) string {
	pwd := []byte(password)
	salt := getSalt()
	shaPwd := base64.StdEncoding.EncodeToString(pbkdf2.Key(pwd, salt, iterations, keyLen, sha256.New))
	return tag + separator + algorithm + separator + strconv.Itoa(iterations) + separator + string(salt) + separator + shaPwd
}

func getSalt() []byte {
	return []byte(randString(saltLen))
}

func randString(length int) string {
	rand.Seed(time.Now().UnixNano())
	rs := make([]string, length)
	for start := 0; start < length; start++ {
		t := rand.Intn(3)
		if t == 0 {
			rs = append(rs, strconv.Itoa(rand.Intn(10)))
		} else if t == 1 {
			rs = append(rs, string(rand.Intn(26)+65))
		} else {
			rs = append(rs, string(rand.Intn(26)+97))
		}
	}
	return strings.Join(rs, "")
}
func main() {
	fmt.Println(CratePassword("123456"))
}