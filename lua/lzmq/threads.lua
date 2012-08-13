-- Copyright (c) 2011 by Robert G. Jakabosky <bobby@sharedrealm.com>
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

--
-- zmq.thread wraps the low-level threads object & a zmq context.
--

local zmq = require"lzmq"
local Threads = require"llthreads.ex"

local zthreads_prelude = [[
local zmq = require"zmq3"
local zthreads = require"zmq3.threads"
local parent_ctx = arg[1]
if parent_ctx then zthreads.set_parent_ctx(zmq.init_ctx(parent_ctx)) end
arg = { select(2, unpack(arg)) }
]]

local M = {}

function M.runfile(ctx, file, ...)
	if ctx then ctx = ctx:lightuserdata() end
	return Threads.runfile_ex(zthreads_prelude, file, ctx, ...)
end

function M.runstring(ctx, code, ...)
	if ctx then ctx = ctx:lightuserdata() end
	return Threads.runstring_ex(zthreads_prelude, code, ctx, ...)
end

local parent_ctx = nil
function M.set_parent_ctx(ctx)
	parent_ctx = ctx
end

function M.get_parent_ctx(ctx)
	return parent_ctx
end

return M
