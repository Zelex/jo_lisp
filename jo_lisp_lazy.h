#pragma once

// TODO: redo all this stuff using native lambda functions.

// (range)
// (range end)
// (range start end)
// (range start end step)
// Returns a lazy seq of nums from start (inclusive) to end
// (exclusive), by step, where start defaults to 0, step to 1, and end to
// infinity. When step is equal to 0, returns an infinite sequence of
// start. When start is equal to end, returns empty list.
static node_idx_t native_range(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	int end = args->size(), start = 0, step = 1;
	if(end == 0) {
		end = INT_MAX; // "infinite" series
	} else if(end == 1) {
		end = get_node(*it++)->as_int();
	} else if(end == 2) {
		start = get_node(*it++)->as_int();
		end = get_node(*it++)->as_int();
	} else if(end == 3) {
		start = get_node(*it++)->as_int();
		end = get_node(*it++)->as_int();
		step = get_node(*it++)->as_int();
	}
	// @ Maybe make 32 configurable
	if(end != INT_MAX && (end - start) / step < 32) {
		list_ptr_t ret = new_list();
		for(int i = start; i < end; i += step) {
			ret->push_back_inplace(new_node_int(i));
		}
		return new_node_list(ret);
	}
	// constructs a function which returns the next value in the range, and another function
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("range-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(new_node_int(start));
	get_node(lazy_func_idx)->t_list->push_back_inplace(new_node_int(step));
	get_node(lazy_func_idx)->t_list->push_back_inplace(new_node_int(end));
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_range_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	int start = get_node(*it++)->as_int();
	int step = get_node(*it++)->as_int();
	int end = get_node(*it++)->as_int();
	if(start >= end) {
		return NIL_NODE;
	}
	list_ptr_t ret = new_list();
	// this value
	ret->push_back_inplace(new_node_int(start));
	// next function
	ret->push_back_inplace(env->get("range-next"));
	ret->push_back_inplace(new_node_int(start+step));
	ret->push_back_inplace(new_node_int(step));
	ret->push_back_inplace(new_node_int(end));
	return new_node_list(ret);
}

// (repeat x)
// (repeat n x)
// Returns a lazy (infinite!, or length n if supplied) sequence of xs.
static node_idx_t native_repeat(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t x;
	int n = INT_MAX;
	if(args->size() == 1) {
		x = eval_node(env, *it++);
	} else if(args->size() == 2) {
		n = get_node(eval_node(env, *it++))->as_int();
		x = eval_node(env, *it++);
	} else {
		return NIL_NODE;
	}
	// @ Maybe make 32 configurable
	if(n <= 32) {
		list_ptr_t ret = new_list();
		for(int i = 0; i < n; i++) {
			ret->push_back_inplace(x);
		}
		return new_node_list(ret);
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("repeat-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(x);
	get_node(lazy_func_idx)->t_list->push_back_inplace(new_node_int(n));
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_repeat_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t x = *it++;
	int n = get_node(*it++)->as_int();
	if(n <= 0) {
		return NIL_NODE;
	}
	list_ptr_t ret = new_list();
	ret->push_back_inplace(x);
	ret->push_back_inplace(env->get("repeat-next"));
	ret->push_back_inplace(x);
	ret->push_back_inplace(new_node_int(n-1));
	return new_node_list(ret);
}

// (concat)
// (concat x) 
// (concat x y) 
// (concat x y & zs)
// Returns a lazy seq representing the concatenation of the elements in the supplied colls.
static node_idx_t native_concat(env_ptr_t env, list_ptr_t args) {
	if(args->size() == 0) {
		return NIL_NODE;
	}
	return new_node_lazy_list(new_node_list(args->push_front(env->get("concat-next"))));
}

static node_idx_t native_concat_next(env_ptr_t env, list_ptr_t args) {
concat_next:
	if(args->size() == 0) {
		return NIL_NODE;
	}
	node_idx_t nidx = eval_node(env, args->first_value());
	node_idx_t val = NIL_NODE;
	int ntype = get_node(nidx)->type;
	args = args->rest();
	if(ntype == NODE_NIL) {
		goto concat_next;
	} else if(ntype == NODE_LIST) {
		list_ptr_t n = get_node(nidx)->t_list;
		if(n->size() == 0) {
			goto concat_next;
		}
		val = n->first_value();
		args->cons_inplace(new_node_list(n->pop()));
	} else if(ntype == NODE_VECTOR) {
		vector_ptr_t n = get_node(nidx)->t_vector;
		if(n->size() == 0) {
			goto concat_next;
		}
		val = n->first_value();
		args->cons_inplace(new_node_vector(n->pop_front()));
	} else if(ntype == NODE_LAZY_LIST) {
		// call the t_lazy_fn, and grab the first element of the return and return that.
		node_idx_t reti = eval_node(env, get_node(nidx)->t_lazy_fn);
		if(reti == NIL_NODE) {
			goto concat_next;
		}
		node_t *ret = get_node(reti);
		if(ret->is_list()) {
			list_ptr_t list_list = ret->as_list();
			val = list_list->first_value();
			args->cons_inplace(new_node_lazy_list(new_node_list(list_list->rest())));
		}
	} else if(ntype == NODE_STRING) {
		// pull off the first character of the string
		jo_string str = get_node(nidx)->t_string;
		if(str.size() == 0) {
			goto concat_next;
		}
		val = new_node_string(str.substr(0, 1));
		args->cons_inplace(new_node_string(str.substr(1)));
	} else {
		val = nidx;
	}
	list_ptr_t ret = new_list();
	ret->push_back_inplace(val);
	ret->push_back_inplace(env->get("concat-next"));
	ret->conj_inplace(*args.ptr);
	return new_node_list(ret);
}

// (iterate f x)
// Returns a lazy seq representing the infinite sequence of x, f(x), f(f(x)), etc.
// f must be free of side-effects
static node_idx_t native_iterate(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f = *it++;
	node_idx_t x = *it++;
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("iterate-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(f);
	get_node(lazy_func_idx)->t_list->push_back_inplace(x);
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_iterate_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f = *it++;
	node_idx_t x = *it++;
	list_ptr_t ret = new_list();
	ret->push_back_inplace(x);
	ret->push_back_inplace(env->get("iterate-next"));
	ret->push_back_inplace(f);
	ret->push_back_inplace(eval_va(env, 2, f, x));
	return new_node_list(ret);
}

// (map f)
// (map f coll)
// (map f c1 c2)
// (map f c1 c2 c3)
// (map f c1 c2 c3 & colls)
// Returns a lazy sequence consisting of the result of applying f to
// the set of first items of each coll, followed by applying f to the
// set of second items in each coll, until any one of the colls is
// exhausted.  Any remaining items in other colls are ignored. Function
// f should accept number-of-colls arguments. Returns a transducer when
// no collection is provided.
static node_idx_t native_map(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f = *it++;
	if(args->size() == 1) {
		node_idx_t lazy_func_idx = new_node(NODE_LIST);
		node_t *lazy_func = get_node(lazy_func_idx);
		lazy_func->t_list = new_list();
		lazy_func->t_list->push_back_inplace(env->get("map-next"));
		lazy_func->t_list->push_back_inplace(f);
		return new_node_lazy_list(lazy_func_idx);
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("map-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(f);
	while(it) {
		get_node(lazy_func_idx)->t_list->push_back_inplace(eval_node(env, *it++));
	}
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_map_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f = *it++;
	// pull off the first element of each list and call f with it
	list_ptr_t next_list = new_list();
	list_ptr_t arg_list = new_list();
	arg_list->push_back_inplace(f);
	next_list->push_back_inplace(env->get("map-next"));
	next_list->push_back_inplace(f);
	for(; it; it++) {
		node_idx_t arg_idx = *it;
		node_t *arg = get_node(arg_idx);
		if(arg->is_list()) {
			list_ptr_t list_list = arg->as_list();
			if(list_list->size() == 0) {
				return NIL_NODE;
			}
			arg_list->push_back_inplace(list_list->first_value());
			next_list->push_back_inplace(new_node_list(list_list->rest()));
		} else if(arg->is_vector()) {
			vector_ptr_t list_list = arg->as_vector();
			if(list_list->size() == 0) {
				return NIL_NODE;
			}
			arg_list->push_back_inplace(list_list->first_value());
			next_list->push_back_inplace(new_node_vector(list_list->rest()));
		} else if(arg->is_lazy_list()) {
			lazy_list_iterator_t lit(env, arg_idx);
			if(lit.done()) {
				return NIL_NODE;
			}
			arg_list->push_back_inplace(lit.val);
			next_list->push_back_inplace(new_node_lazy_list(lit.next_fn()));
		}
	}
	// call f with the args
	node_idx_t ret = eval_list(env, arg_list);
	next_list->cons_inplace(ret);
	return new_node_list(next_list);
}

// (take n coll)
// Returns a lazy sequence of the first n items in coll, or all items if there are fewer than n.
static node_idx_t native_take(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n = eval_node(env, *it++);
	node_idx_t coll = eval_node(env, *it++);
	if(get_node_type(coll) == NODE_LIST) {
		// don't do it lazily if not given lazy inputs... thats dumb
		int N = get_node(n)->as_int();
		list_ptr_t list_list = get_node(coll)->as_list();
		if(list_list->size() <= N) {
			return coll;
		}
		return new_node_list(list_list->take(N));
	}
	if(get_node_type(coll) == NODE_VECTOR) {
		// don't do it lazily if not given lazy inputs... thats dumb
		int N = get_node(n)->as_int();
		vector_ptr_t list_list = get_node(coll)->as_vector();
		if(list_list->size() <= N) {
			return coll;
		}
		return new_node_vector(list_list->take(N));
	}
	if(get_node_type(coll) == NODE_LAZY_LIST) {
		node_idx_t lazy_func_idx = new_node(NODE_LIST);
		get_node(lazy_func_idx)->t_list = new_list();
		get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("take-next"));
		get_node(lazy_func_idx)->t_list->push_back_inplace(n);
		get_node(lazy_func_idx)->t_list->push_back_inplace(coll);
		return new_node_lazy_list(lazy_func_idx);
	}
	return coll;
}

static node_idx_t native_take_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	int n = get_node(*it++)->as_int();
	if(n <= 0) {
		return NIL_NODE;
	}
	node_idx_t coll = *it++;
	lazy_list_iterator_t lit(env, coll);
	if(lit.done()) {
		return NIL_NODE;
	}
	list_ptr_t list = new_list();
	list->push_back_inplace(lit.val);
	list->push_back_inplace(env->get("take-next"));
	list->push_back_inplace(new_node_int(n-1));
	list->push_back_inplace(new_node_lazy_list(lit.next_fn()));
	return new_node_list(list);
}

// (take-nth n) (take-nth n coll)
// Returns a lazy seq of every nth item in coll.  Returns a stateful
// transducer when no collection is provided.
static node_idx_t native_take_nth(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n = eval_node(env, *it++);
	if(!it) {
		// stateful transducer
		node_idx_t lazy_func_idx = new_node(NODE_LIST);
		get_node(lazy_func_idx)->t_list = new_list();
		get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("take-nth-next"));
		get_node(lazy_func_idx)->t_list->push_back_inplace(n);
		return new_node_lazy_list(lazy_func_idx);
	}
	int N = get_node(n)->as_int();
	node_idx_t coll = eval_node(env, *it++);
	if(get_node_type(coll) == NODE_LIST) {
		// don't do it lazily if not given lazy inputs... thats dumb
		list_ptr_t list_list = get_node(coll)->as_list();
		if(N <= 0) {
			// (take-nth 0 coll) will return an infinite sequence repeating for first item from coll. A negative N is treated the same as 0.
			node_idx_t lazy_func_idx = new_node(NODE_LIST);
			get_node(lazy_func_idx)->t_list = new_list();
			get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("constantly-next"));
			get_node(lazy_func_idx)->t_list->push_back_inplace(list_list->first_value());
			return new_node_lazy_list(lazy_func_idx);
		}
		list_ptr_t list = new_list();
		if(list_list->size() <= N) {
			list->push_back_inplace(list_list->first_value());
			return new_node_list(list);
		}
		for(list_t::iterator it = list_list->begin(); it; it += N) {
			list->push_back_inplace(*it);
		}
		return new_node_list(list);
	}
	if(get_node_type(coll) == NODE_VECTOR) {
		// don't do it lazily if not given lazy inputs... thats dumb
		vector_ptr_t list_list = get_node(coll)->as_vector();
		if(N <= 0) {
			// (take-nth 0 coll) will return an infinite sequence repeating for first item from coll. A negative N is treated the same as 0.
			node_idx_t lazy_func_idx = new_node(NODE_LIST);
			get_node(lazy_func_idx)->t_list = new_list();
			get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("constantly-next"));
			get_node(lazy_func_idx)->t_list->push_back_inplace(list_list->first_value());
			return new_node_lazy_list(lazy_func_idx);
		}
		vector_ptr_t list = new_vector();
		if(list_list->size() <= N) {
			list->push_back_inplace(list_list->first_value());
			return new_node_vector(list);
		}
		for(vector_t::iterator it = list_list->begin(); it; it += N) {
			list->push_back_inplace(*it);
		}
		return new_node_vector(list);
	}
	if(get_node_type(coll) == NODE_LAZY_LIST) {
		if(N <= 0) {
			lazy_list_iterator_t lit(env, coll);
			// (take-nth 0 coll) will return an infinite sequence repeating for first item from coll. A negative N is treated the same as 0.
			node_idx_t lazy_func_idx = new_node(NODE_LIST);
			get_node(lazy_func_idx)->t_list = new_list();
			get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("constantly-next"));
			get_node(lazy_func_idx)->t_list->push_back_inplace(lit.val);
			return new_node_lazy_list(lazy_func_idx);
		}
		node_idx_t lazy_func_idx = new_node(NODE_LIST);
		get_node(lazy_func_idx)->t_list = new_list();
		get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("take-nth-next"));
		get_node(lazy_func_idx)->t_list->push_back_inplace(n);
		get_node(lazy_func_idx)->t_list->push_back_inplace(coll);
		return new_node_lazy_list(lazy_func_idx);
	}
	return coll;
}

static node_idx_t native_take_nth_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	int n = get_node(*it++)->as_int();
	if(n <= 0) {
		return NIL_NODE;
	}
	node_idx_t coll = *it++;
	lazy_list_iterator_t lit(env, coll);
	if(lit.done()) {
		return NIL_NODE;
	}
	list_ptr_t list = new_list();
	list->push_back_inplace(lit.val);
	list->push_back_inplace(env->get("take-nth-next"));
	list->push_back_inplace(new_node_int(n));
	list->push_back_inplace(new_node_lazy_list(lit.next_fn(n)));
	return new_node_list(list);
}

static node_idx_t native_constantly_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t value = *it++;
	list_ptr_t list = new_list();
	list->push_back_inplace(value);
	list->push_back_inplace(env->get("constantly-next"));
	list->push_back_inplace(value);
	return new_node_list(list);
}

// (take-last n coll)
// Returns a seq of the last n items in coll.  Depending on the type
// of coll may be no better than linear time.  For vectors, see also subvec.
static node_idx_t native_take_last(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n = eval_node(env, *it++);
	node_idx_t coll = eval_node(env, *it++);
	int N = get_node(n)->as_int();
	if(get_node_type(coll) == NODE_LIST) {
		list_ptr_t list_list = get_node(coll)->as_list();
		if(list_list->size() <= N) {
			return coll;
		}
		return new_node_list(list_list->take_last(N));
	}
	if(get_node_type(coll) == NODE_VECTOR) {
		vector_ptr_t list_list = get_node(coll)->as_vector();
		if(list_list->size() <= N) {
			return coll;
		}
		return new_node_vector(list_list->take_last(N));
	}
	if(get_node_type(coll) == NODE_LAZY_LIST) {
		lazy_list_iterator_t lit(env, coll);
		list_ptr_t ret = new_list();
		for(; lit; lit.next()) {
			node_idx_t value_idx = eval_node(env, lit.val);
			node_t *value = get_node(value_idx);
			ret->push_back_inplace(value_idx);
			if(ret->size() > N) {
				ret->pop_front_inplace();
			}
		}
		return new_node_list(ret);
	}
	return coll;
}

// (distinct)
// (distinct coll)
// Returns a lazy sequence of the elements of coll with duplicates removed.
// Returns a stateful transducer when no collection is provided.
static node_idx_t native_distinct(env_ptr_t env, list_ptr_t args) {
	// TODO: lazy? How is that a good idea for distinct? I think it's a bad idea.
	list_t::iterator it = args->begin();
	node_idx_t node_idx = *it++;
	node_t *node = get_node(node_idx);
	if(node->is_list()) {
		list_ptr_t list_list = node->as_list();
		list_ptr_t ret = new_list();
		for(list_t::iterator it = list_list->begin(); it; it++) {
			node_idx_t value_idx = eval_node(env, *it);
			node_t *value = get_node(value_idx);
			if(!ret->contains([env,value_idx](node_idx_t idx) {
				return node_eq(env, idx, value_idx);
			})) {
				ret->push_back_inplace(value_idx);
			}
		}
		return new_node_list(ret, NODE_FLAG_LITERAL);
	}
	if(node->is_vector()) {
		vector_ptr_t vector_list = node->as_vector();
		vector_ptr_t ret = new_vector();
		for(vector_t::iterator it = vector_list->begin(); it; it++) {
			node_idx_t value_idx = eval_node(env, *it);
			node_t *value = get_node(value_idx);
			if(!ret->contains([env,value_idx](node_idx_t idx) {
				return node_eq(env, idx, value_idx);
			})) {
				ret->push_back_inplace(value_idx);
			}
		}
		return new_node_vector(ret, NODE_FLAG_LITERAL);
	}
	if(node->is_lazy_list()) {
		lazy_list_iterator_t lit(env, node_idx);
		list_ptr_t ret = new_list();
		for(; lit; lit.next()) {
			node_idx_t value_idx = eval_node(env, lit.val);
			node_t *value = get_node(value_idx);
			if(!ret->contains([env,value_idx](node_idx_t idx) {
				return node_eq(env, idx, value_idx);
			})) {
				ret->push_back_inplace(value_idx);
			}
		}
		return new_node_list(ret);
	}
	return NIL_NODE;
}

// (filter pred)(filter pred coll)
// Returns a lazy sequence of the items in coll for which
// (pred item) returns logical true. pred must be free of side-effects.
// Returns a transducer when no collection is provided.
static node_idx_t native_filter(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t pred_idx = eval_node(env, *it++);
	node_idx_t coll_idx = eval_node(env, *it++);
	//print_node(coll_idx);
	if(get_node_type(coll_idx) == NODE_LIST) {
		// don't do it lazily if not given lazy inputs... thats dumb
		list_ptr_t list_list = get_node(coll_idx)->as_list();
		list_ptr_t ret = new_list();
		list_ptr_t args = new_list();
		args->push_back_inplace(pred_idx);
		for(list_t::iterator it = list_list->begin(); it; it++) {
			node_idx_t item_idx = *it;//eval_node(env, *it);
			//print_node(item_idx);
			node_idx_t comp = eval_list(env, args->conj(item_idx));
			if(get_node_bool(comp)) {
				ret->push_back_inplace(item_idx);
			}
		}
		return new_node_list(ret);
	}
	if(get_node_type(coll_idx) == NODE_VECTOR) {
		vector_ptr_t vector_list = get_node(coll_idx)->as_vector();
		vector_ptr_t ret = new_vector();
		list_ptr_t args = new_list();
		args->push_back_inplace(pred_idx);
		for(vector_t::iterator it = vector_list->begin(); it; it++) {
			node_idx_t item_idx = *it;//eval_node(env, *it);
			//print_node(item_idx);
			node_idx_t comp = eval_list(env, args->conj(item_idx));
			if(get_node_bool(comp)) {
				ret->push_back_inplace(item_idx);
			}
		}
		return new_node_vector(ret);
	}
	if(get_node_type(coll_idx) == NODE_MAP) {
		// don't do it lazily if not given lazy inputs... thats dumb
		map_ptr_t list_list = get_node(coll_idx)->as_map();
		map_ptr_t ret = new_map();
		list_ptr_t args = new_list();
		args->push_back_inplace(pred_idx);
		for(map_t::iterator it = list_list->begin(); it; it++) {
			list_ptr_t key_val = new_list();
			key_val->push_back_inplace(it->first);
			key_val->push_back_inplace(it->second);
			node_idx_t comp = eval_list(env, args->conj(new_node_list(key_val)));
			if(get_node_bool(comp)) {
				ret->assoc_inplace(it->first, it->second);
			}
		}
		return new_node_map(ret);
	}
	if(get_node_type(coll_idx) == NODE_LAZY_LIST) {
		node_idx_t lazy_func_idx = new_node(NODE_LIST);
		get_node(lazy_func_idx)->t_list = new_list();
		get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("filter-next"));
		get_node(lazy_func_idx)->t_list->push_back_inplace(pred_idx);
		get_node(lazy_func_idx)->t_list->push_back_inplace(coll_idx);
		return new_node_lazy_list(lazy_func_idx);
	}
	if(get_node_type(coll_idx) == NODE_STRING) {
		jo_string str = get_node(coll_idx)->t_string;
		jo_string ret;
		list_ptr_t args = new_list();
		args->push_back_inplace(pred_idx);
		size_t str_len = str.length();
		const char *str_ptr = str.c_str();
		for(int i = 0; i < str_len; i++) {
			node_idx_t item_idx = new_node_int(str_ptr[i]);
			node_idx_t comp = eval_list(env, args->conj(item_idx));
			if(get_node_bool(comp)) {
				ret += (char)str_ptr[i];
			}
		}
		return new_node_string(ret);
	}
	return NIL_NODE;
}

static node_idx_t native_filter_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t pred_idx = *it++;
	node_idx_t coll_idx = *it++;
	list_ptr_t e = new_list();
	e->push_back_inplace(pred_idx);
	for(lazy_list_iterator_t lit(env, coll_idx); !lit.done(); lit.next()) {
		node_idx_t comp = eval_list(env, e->conj(lit.val));
		if(get_node_bool(comp)) {
			list_ptr_t ret = new_list();
			ret->push_back_inplace(lit.val);
			ret->push_back_inplace(env->get("filter-next"));
			ret->push_back_inplace(pred_idx);
			ret->push_back_inplace(new_node_lazy_list(lit.next_fn()));
			return new_node_list(ret);
		}
	}
	return NIL_NODE;
}

// (keep f)(keep f coll)
// Returns a lazy sequence of the non-nil results of (f item). Note,
// this means false return values will be included.  f must be free of
// side-effects.  Returns a transducer when no collection is provided.
static node_idx_t native_keep(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f_idx = eval_node(env, *it++);
	node_idx_t coll_idx = eval_node(env, *it++);
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("keep-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(f_idx);
	get_node(lazy_func_idx)->t_list->push_back_inplace(coll_idx);
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_keep_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f_idx = *it++;
	node_idx_t coll_idx = *it++;
	int coll_type = get_node_type(coll_idx);
	list_ptr_t e = new_list();
	e->push_back_inplace(f_idx);
	if(coll_type == NODE_LIST) {
		list_ptr_t list_list = get_node(coll_idx)->t_list;
		while(!list_list->empty()) {
			node_idx_t item_idx = list_list->first_value();
			list_list = list_list->pop_front();
			node_idx_t comp = eval_list(env, e->conj(item_idx));
			if(comp != NIL_NODE) {
				list_ptr_t ret = new_list();
				ret->push_back_inplace(comp);
				ret->push_back_inplace(env->get("keep-next"));
				ret->push_back_inplace(f_idx);
				ret->push_back_inplace(new_node_list(list_list));
				return new_node_list(ret);
			}
		}
	}
	if(coll_type == NODE_VECTOR) {
		vector_ptr_t vec_list = get_node(coll_idx)->t_vector;
		while(!vec_list->empty()) {
			node_idx_t item_idx = vec_list->first_value();
			vec_list = vec_list->pop_front();
			node_idx_t comp = eval_list(env, e->conj(item_idx));
			if(comp != NIL_NODE) {
				list_ptr_t ret = new_list();
				ret->push_back_inplace(comp);
				ret->push_back_inplace(env->get("keep-next"));
				ret->push_back_inplace(f_idx);
				ret->push_back_inplace(new_node_vector(vec_list));
				return new_node_list(ret);
			}
		}
	}
	if(coll_type == NODE_LAZY_LIST) {
		for(lazy_list_iterator_t lit(env, coll_idx); !lit.done(); lit.next()) {
			node_idx_t item_idx = lit.val;
			node_idx_t comp = eval_list(env, e->conj(item_idx));
			if(comp != NIL_NODE) {
				list_ptr_t ret = new_list();
				ret->push_back_inplace(comp);
				ret->push_back_inplace(env->get("keep-next"));
				ret->push_back_inplace(f_idx);
				ret->push_back_inplace(new_node_lazy_list(lit.next_fn()));
				return new_node_list(ret);
			}
		}
	}
	return NIL_NODE;
}

// (repeatedly f)(repeatedly n f)
// Takes a function of no args, presumably with side effects, and
// returns an infinite (or length n if supplied) lazy sequence of calls
// to it
static node_idx_t native_repeatedly(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n_idx, f_idx;
	if(args->size() == 1) {
		n_idx = new_node_int(INT_MAX);
		f_idx = eval_node(env, *it++);
	} else {
		n_idx = eval_node(env, *it++);
		f_idx = eval_node(env, *it++);
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("repeatedly-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(f_idx);
	get_node(lazy_func_idx)->t_list->push_back_inplace(n_idx);
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_repeatedly_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t f_idx = *it++;
	node_idx_t n_idx = *it++;
	int n = get_node_int(n_idx);
	if(n > 0) {
		list_ptr_t ret_list = new_list();
		ret_list->push_back_inplace(eval_va(env, 1, f_idx));
		ret_list->push_back_inplace(env->get("repeatedly-next"));
		ret_list->push_back_inplace(f_idx);
		ret_list->push_back_inplace(new_node_int(n - 1));
		return new_node_list(ret_list);
	}
	return NIL_NODE;
}

// (partition n coll)(partition n step coll)(partition n step pad coll)
// Returns a lazy sequence of lists of n items each, at offsets step
// apart. If step is not supplied, defaults to n, i.e. the partitions
// do not overlap. If a pad collection is supplied, use its elements as
// necessary to complete last partition upto n items. In case there are
// not enough padding elements, return a partition with less than n items.
static node_idx_t native_partition(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n_idx, step_idx, pad_idx, coll_idx;
	if(args->size() == 1) {
		return *it;
	} else if(args->size() == 2) {
		n_idx = eval_node(env, *it++);
		step_idx = n_idx;
		pad_idx = NIL_NODE;
		coll_idx = eval_node(env, *it++);
	} else if(args->size() == 3) {
		n_idx = eval_node(env, *it++);
		step_idx = eval_node(env, *it++);
		pad_idx = NIL_NODE;
		coll_idx = eval_node(env, *it++);
	} else {
		n_idx = eval_node(env, *it++);
		step_idx = eval_node(env, *it++);
		pad_idx = eval_node(env, *it++);
		coll_idx = eval_node(env, *it++);
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("partition-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(n_idx);
	get_node(lazy_func_idx)->t_list->push_back_inplace(step_idx);
	get_node(lazy_func_idx)->t_list->push_back_inplace(pad_idx);
	get_node(lazy_func_idx)->t_list->push_back_inplace(coll_idx);
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_partition_next(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n_idx = *it++;
	node_idx_t step_idx = *it++;
	node_idx_t pad_idx = *it++;
	node_idx_t coll_idx = *it++;
	int n = get_node_int(n_idx);
	int step = get_node_int(step_idx);
	int coll_type = get_node_type(coll_idx);
	node_idx_t new_coll = NIL_NODE;
	list_ptr_t ret;
	vector_ptr_t retv;
	if(coll_type == NODE_LIST) {
		ret = get_node_list(coll_idx)->take(n);
		new_coll = new_node_list(get_node_list(coll_idx)->drop(step));
	} else if(coll_type == NODE_VECTOR) {
		retv = get_node_vector(coll_idx)->take(n);
		new_coll = new_node_vector(get_node_vector(coll_idx)->drop(step));
	} else if(coll_type == NODE_LAZY_LIST) {
		lazy_list_iterator_t lit(env, coll_idx);
		if(step == n) {
			ret = lit.all(n);
			new_coll = new_node_lazy_list(lit.next_fn());
		} else if(step < n) {
			ret = lit.all(step);
			new_coll = new_node_lazy_list(lit.next_fn());
			if(lit.val != INV_NODE && n - step > 0) {
				ret = ret->conj(*lit.all(n - step));
			}
		} else if(step > n) {
			ret = lit.all(n);
			lit.all(step - n);
			new_coll = new_node_lazy_list(lit.next_fn());
		}
	} else {
		printf("partition: collection type not supported\n");
		return NIL_NODE;
	}
	if((!ret.ptr || ret->size() == 0) && (!retv.ptr || retv->size() == 0)) {
		//printf("partition: not enough elements in collection\n");
		return NIL_NODE;
	}
	if((!ret.ptr || ret->size() < n) && (!retv.ptr || retv->size() < n)) {
		if(pad_idx != NIL_NODE) {
			int pad_n = n - ret->size();
			int pad_type = get_node_type(pad_idx);
			list_ptr_t pad_coll;
			if(pad_type == NODE_LIST) {
				pad_coll = get_node_list(pad_idx)->take(pad_n);
			} else if(pad_type == NODE_LAZY_LIST) {
				pad_coll = lazy_list_iterator_t(env, pad_idx).all(pad_n);
			} else {
				return NIL_NODE;
			}
			ret = ret->conj(*pad_coll);
		} else {
			return NIL_NODE;
		}
	}
	//printf("("); print_node_list(ret); printf(")\n");
	list_ptr_t ret_list = new_list();
	if(retv.ptr) {
		ret_list->push_back_inplace(new_node_vector(retv));
	} else {
		ret_list->push_back_inplace(new_node_list(ret));
	}
	ret_list->push_back_inplace(env->get("partition-next"));
	ret_list->push_back_inplace(n_idx);
	ret_list->push_back_inplace(step_idx);
	ret_list->push_back_inplace(pad_idx);
	ret_list->push_back_inplace(new_coll);
	return new_node_list(ret_list);
}

static node_idx_t native_drop(env_ptr_t env, list_ptr_t args) {
	list_t::iterator it = args->begin();
	node_idx_t n_idx = *it++;
	int n = get_node(n_idx)->as_int();
	node_idx_t list_idx = *it++;
	if(n <= 0) {
		return list_idx;
	}
	node_t *list = get_node(list_idx);
	list_ptr_t list_list = list->as_list();
	if(list->is_string()) {
		jo_string &str = list->t_string;
		if(n < 0) {
			n = str.size() + n;
		}
		if(n < 0 || n >= str.size()) {
			return new_node_string("");
		}
		return new_node_string(str.substr(n));
	}
	if(list->is_list()) {
		return new_node_list(list_list->drop(n));
	}
	if(list->is_vector()) {
		return new_node_vector(list->as_vector()->drop(n));
	}
	if(list->is_lazy_list()) {
		lazy_list_iterator_t lit(env, list_idx);
		lit.nth(n-1);
		if(lit.done()) {
			return NIL_NODE;
		}
		return new_node_lazy_list(lit.next_fn());
	}
	return NIL_NODE;
}

// (interleave)(interleave c1)(interleave c1 c2)(interleave c1 c2 & colls)
// Returns a lazy seq of the first item in each coll, then the second etc.
// (interleave [:a :b :c] [1 2 3]) => (:a 1 :b 2 :c 3)
static node_idx_t native_interleave(env_ptr_t env, list_ptr_t args) {
	if(args->size() == 0) {
		return NIL_NODE;
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	get_node(lazy_func_idx)->t_list = new_list();
	get_node(lazy_func_idx)->t_list->push_back_inplace(env->get("interleave-next"));
	get_node(lazy_func_idx)->t_list->push_back_inplace(ZERO_NODE);
	get_node(lazy_func_idx)->t_list->conj_inplace(*args.ptr);
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_interleave_next(env_ptr_t env, list_ptr_t args) {
	if(args->size() == 0) {
		return NIL_NODE;
	}
	node_idx_t coll_idx = args->first_value();
	if(coll_idx == ZERO_NODE) {
		// check if any of args are done
		for(list_t::iterator it = args->begin()+1; it; ++it) {
			node_t *n = get_node(*it);
			int ntype = n->type;
			if(ntype == NODE_NIL) {
				return NIL_NODE;
			} else if(ntype == NODE_LIST) {
				if(n->t_list->size() == 0) {
					return NIL_NODE;
				}
			} else if(ntype == NODE_VECTOR) {
				if(n->t_vector->size() == 0) {
					return NIL_NODE;
				}
			} else if(ntype == NODE_LAZY_LIST) {
				if(eval_node(env, n->t_lazy_fn) == NIL_NODE) {
					return NIL_NODE;
				}
			} else if(ntype == NODE_STRING) {
				if(n->t_string.size() == 0) {
					return NIL_NODE;
				}
			} else {
				return NIL_NODE;
			}
		}
	}
	node_idx_t nidx = args->second_value();
	node_idx_t val = NIL_NODE;
	int ntype = get_node(nidx)->type;
	args = args->rest(args->begin()+2);
	if(ntype == NODE_LIST) {
		list_ptr_t n = get_node(nidx)->t_list;
		val = n->first_value();
		args->cons_inplace(new_node_list(n->pop()));
	} else if(ntype == NODE_VECTOR) {
		vector_ptr_t n = get_node(nidx)->t_vector;
		val = n->first_value();
		args->cons_inplace(new_node_vector(n->pop_front()));
	} else if(ntype == NODE_LAZY_LIST) {
		// call the t_lazy_fn, and grab the first element of the return and return that.
		node_idx_t reti = eval_node(env, get_node(nidx)->t_lazy_fn);
		node_t *ret = get_node(reti);
		if(ret->is_list()) {
			list_ptr_t list_list = ret->as_list();
			val = list_list->first_value();
			args->cons_inplace(new_node_lazy_list(new_node_list(list_list->rest())));
		}
	} else if(ntype == NODE_STRING) {
		// pull off the first character of the string
		jo_string str = get_node(nidx)->t_string;
		val = new_node_string(str.substr(0, 1));
		args->cons_inplace(new_node_string(str.substr(1)));
	}
	list_ptr_t ret = new_list();
	ret->push_back_inplace(val);
	ret->push_back_inplace(env->get("interleave-next"));
	int next_coll_it = get_node_int(coll_idx)+1;
	if(args->size() == next_coll_it) {
		ret->push_back_inplace(ZERO_NODE);
	} else {
		ret->push_back_inplace(new_node_int(next_coll_it));
	}
	ret->conj_inplace(*args->rest()->clone());
	ret->push_back_inplace(args->first_value());
	return new_node_list(ret);
}

// (flatten x)
// Takes any nested combination of sequential things (lists, vectors,
// etc.) and returns their contents as a single, flat lazy sequence.
// (flatten nil) returns an empty sequence.
static node_idx_t native_flatten(env_ptr_t env, list_ptr_t args) {
	node_idx_t x = args->first_value();
	if(x <= NIL_NODE) {
		return EMPTY_LIST_NODE;
	}
	node_t *n = get_node(x);
	if(!n->is_seq()) {
		return x;
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	node_t *lazy_func = get_node(lazy_func_idx);
	lazy_func->t_list = new_list();
	lazy_func->t_list->push_front_inplace(get_node(x)->seq_rest(env));
	while(n->is_seq()) {
		x = n->seq_first(env);
		n = get_node(x);
		if(n->is_seq()) {
			lazy_func->t_list->push_front_inplace(get_node(x)->seq_rest(env));
		} else {
			lazy_func->t_list->push_front_inplace(x);
		}
	}
	lazy_func->t_list->push_front_inplace(env->get("flatten-next"));
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_flatten_next(env_ptr_t env, list_ptr_t args) {
	if(args->size() == 0) {
		return NIL_NODE;
	}

	list_ptr_t ret = new_list();
	node_idx_t ret_value = NIL_NODE;
	
	// first value should always not be a sequence
	ret_value = args->first_value();
	args = args->pop_front();
	assert(!get_node(ret_value)->is_seq());

	node_idx_t seq_idx = args->first_value();
	node_t *seq = get_node(seq_idx);

	ret = ret->push_front(*args->clone());

	if(!seq->is_seq()) {
		ret->push_front_inplace(env->get("flatten-next"));
		ret->push_front_inplace(ret_value);
		return new_node_list(ret);
	}

	do {
		ret = ret->pop();
		auto seq_p = seq->seq_first_rest(env);
		if(!get_node(seq_p.second)->seq_empty(env)) ret->push_front_inplace(seq_p.second);
		if(seq_p.first != NIL_NODE) ret->push_front_inplace(seq_p.first);
		seq_idx = seq_p.first;
		seq = get_node(seq_idx);
	} while(seq->is_seq());

	ret->push_front_inplace(env->get("flatten-next"));
	ret->push_front_inplace(ret_value);
	return new_node_list(ret);
}

// (lazy-seq & body)
// Takes a body of expressions that returns an ISeq or nil, and yields
// a Seqable object that will invoke the body only the first time seq
// is called, and will cache the result and return it on all subsequent
// seq calls. See also - realized?
static node_idx_t native_lazy_seq(env_ptr_t env, list_ptr_t args) {
	if(args->size() == 0) {
		return NIL_NODE;
	}
	node_idx_t lazy_func_idx = new_node(NODE_LIST);
	node_t *lazy_func = get_node(lazy_func_idx);
	lazy_func->t_list = args->push_front(env->get("lazy-seq-first"));
	return new_node_lazy_list(lazy_func_idx);
}

static node_idx_t native_lazy_seq_first(env_ptr_t env, list_ptr_t args) {
	node_idx_t ll = eval_node_list(env, args);
	auto fr = get_node(ll)->seq_first_rest(env);
	list_ptr_t l = new_list();
	l->push_front_inplace(fr.second);
	l->push_front_inplace(env->get("lazy-seq-next"));
	l->push_front_inplace(fr.first);
	return new_node_list(l);
}

static node_idx_t native_lazy_seq_next(env_ptr_t env, list_ptr_t args) {
	auto fr = get_node(args->first_value())->seq_first_rest(env);
	list_ptr_t l = new_list();
	l->push_front_inplace(fr.second);
	l->push_front_inplace(env->get("lazy-seq-next"));
	l->push_front_inplace(fr.first);
	return new_node_list(l);
}


void jo_lisp_lazy_init(env_ptr_t env) {
	env->set("range", new_node_native_function("range", &native_range, false));
	env->set("range-next", new_node_native_function("range-next", &native_range_next, false));
	env->set("repeat", new_node_native_function("repeat", &native_repeat, true));
	env->set("repeat-next", new_node_native_function("repeat-next", &native_repeat_next, true));
	env->set("concat", new_node_native_function("concat", &native_concat, true));
	env->set("concat-next", new_node_native_function("concat-next", &native_concat_next, true));
	env->set("iterate", new_node_native_function("iterate", &native_iterate, true));
	env->set("iterate-next", new_node_native_function("iterate-next", &native_iterate_next, true));
	env->set("map", new_node_native_function("map", &native_map, true));
	env->set("map-next", new_node_native_function("map-next", &native_map_next, true));
	env->set("take", new_node_native_function("take", &native_take, true));
	env->set("take-next", new_node_native_function("take-next", &native_take_next, true));
	env->set("take-nth", new_node_native_function("take-nth", &native_take_nth, true));
	env->set("take-nth-next", new_node_native_function("take-nth-next", &native_take_nth_next, true));
	env->set("take-last", new_node_native_function("take-last", &native_take_last, true));
	env->set("distinct", new_node_native_function("distinct", &native_distinct, false));
	env->set("filter", new_node_native_function("filter", &native_filter, true));
	env->set("filter-next", new_node_native_function("filter-next", &native_filter_next, true));
	env->set("constantly-next", new_node_native_function("constantly-next", &native_constantly_next, true));
	env->set("keep", new_node_native_function("keep", &native_keep, true));
	env->set("keep-next", new_node_native_function("keep-next", &native_keep_next, true));
	env->set("repeatedly", new_node_native_function("repeatedly", &native_repeatedly, true));
	env->set("repeatedly-next", new_node_native_function("repeatedly-next", &native_repeatedly_next, true));
	env->set("partition", new_node_native_function("partition", &native_partition, true));
	env->set("partition-next", new_node_native_function("partition-next", &native_partition_next, true));
	env->set("drop", new_node_native_function("drop", &native_drop, false));
	env->set("interleave", new_node_native_function("interleave", &native_interleave, false));
	env->set("interleave-next", new_node_native_function("interleave-next", &native_interleave_next, true));
	env->set("flatten", new_node_native_function("flatten", &native_flatten, false));
	env->set("flatten-next", new_node_native_function("flatten-next", &native_flatten_next, true));
	//env->set("lazy-seq", new_node_native_function("lazy-seq", &native_lazy_seq, true));
	//env->set("lazy-seq-first", new_node_native_function("lazy-seq-first", &native_lazy_seq_first, true));
	//env->set("lazy-seq-next", new_node_native_function("lazy-seq-next", &native_lazy_seq_next, true));
}
