module MosquittoAuthPlug
  def self.compute_pbkdf2_hmac_sha256(plaintext, salt_length = 12, iterations = 901, key_length = 24)
    salt = SecureRandom.base64(salt_length)

    hash = OpenSSL::PKCS5.pbkdf2_hmac(plaintext, salt, iterations, key_length, OpenSSL::Digest::SHA256.new)
    encoded_hash = Base64.strict_encode64(hash)

    "PBKDF2$sha256$#{iterations}$#{salt}$#{encoded_hash}"
  end
end
