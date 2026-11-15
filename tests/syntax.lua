local count = 0
for path in io.lines(assert(arg[1], "pass a file containing Lua paths")) do
	assert(loadfile(path))
	count = count + 1
end
print("PASS: " .. count .. " mod scripts compile under Lua 5.1")
