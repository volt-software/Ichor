wrk.method = "POST"

-- post form urlencoded data
wrk.body = '{"sequence": 2}'
wrk.headers['Content-Type'] = "application/json"
