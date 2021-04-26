/*
Copyright(c) 2021 Jasper van den Eshof

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <iostream>
#include <vector>

#include "Common.h"

template<typename T> 
class StreamIn
{
public:

	virtual void Receive(const T* data, int len) {}
	virtual void Receive(T* data, int len) 
	{ 
		Receive( (const T *) data, len);  
	}
};

template <typename S> 
class Connection
{
	std::vector<StreamIn<S>*> connections;

public:

	void Send(const S* data, int len)
	{
		for (auto c : connections) c->Receive(data, len);
	}

	void Send(S* data, int len)
	{
		if(connections.size() == 1)
			for (auto c : connections) c->Receive(data, len);
		else
			for (auto c : connections) c->Receive((const S*)data, len);
	}

	void Connect(StreamIn<S>* s)
	{
		connections.push_back(s);
	}
};

template <typename S>
class StreamOut 
{
public:

	Connection<S> out;

	void Send(const S* data, int len)
	{
		out.Send(data, len);
	}

	void Send(S* data, int len)
	{
		out.Send(data, len);
	}
};

template <typename T, typename S>
class SimpleStreamInOut : public StreamOut<S>, public StreamIn<T>
{
public:

	void sendOut(const S* data, int len)
	{
		StreamOut<S>::Send(data, len);
	}

	void sendOut(S* data, int len)
	{
		StreamOut<S>::Send(data, len);
	}
};

template <typename T>
class PassThrough : public SimpleStreamInOut<T,T>
{

public:

	virtual void Receive(const T* data, int len) { SimpleStreamInOut<T,T>::sendOut(data, len);  }
	virtual void Receive(T* data, int len) { SimpleStreamInOut<T, T>::sendOut(data, len); }

};


template <typename T>
class Timer : public SimpleStreamInOut<T, T>
{

	high_resolution_clock::time_point time_start;
	float timing = 0.0;

	void tic()
	{
		time_start = high_resolution_clock::now();
	}

	void toc()
	{
		timing += 1e-3f * duration_cast<microseconds>(high_resolution_clock::now() - time_start).count();
	}

public:

	virtual void Receive(const T* data, int len) { tic();  SimpleStreamInOut<T, T>::sendOut(data, len);  toc();  }
	virtual void Receive(T* data, int len) { tic();  SimpleStreamInOut<T, T>::sendOut(data, len); toc();  }


	float getTotalTiming() { return timing; }
};


template <typename S> 
inline StreamIn<S>& operator>>(Connection<S>& a, StreamIn<S>& b) { a.Connect(&b); return b; }
template <typename T, typename S> 
inline SimpleStreamInOut<S,T>& operator>>(Connection<S>& a, SimpleStreamInOut<S,T>& b) { a.Connect(&b); return b; }

template <typename S>
inline StreamIn<S>& operator>>(StreamOut<S>& a, StreamIn<S>& b) { a.out.Connect(&b); return b; }
template <typename S, typename U>
inline SimpleStreamInOut<S, U>& operator>>(StreamOut<S>& a, SimpleStreamInOut<S, U>& b) { a.out.Connect(&b); return b; }

template <typename T, typename S> 
inline StreamIn<S>& operator>>(SimpleStreamInOut<T,S>& a, StreamIn<S>& b) { a.out.Connect(&b); return b; }
template <typename T, typename S, typename U> 
inline SimpleStreamInOut<S,U>& operator>>(SimpleStreamInOut<T,S>& a, SimpleStreamInOut<S,U>& b) { a.out.Connect(&b); return b; }
