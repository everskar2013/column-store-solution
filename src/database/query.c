#include "query.h"
#define BUFFERSIZE 3096

// Global results hash list to keep track of query results
Result *res_hash_list;

char open_paren[2] = "(";
char close_paren[2] = ")";
char comma[2] = ",";
// char quotes[2] = "\"";
char eq_sign[2] = "=";

/**
 * Grab the result from the result hash list 
 */
status grab_result(const char *res_name, Result **res) {
	status s;
	if (NULL == res_hash_list) {
		log_err("No results to grab from");
		*res = NULL;
		s.code = ERROR;
	}
	else {
		Result *tmp;
		HASH_FIND_STR(res_hash_list, res_name, tmp);
		if (NULL != tmp) {
			*res = tmp;
			s.code = OK;
		}
		else {
			log_err("No result named %s", res_name);
			*res = NULL;
			s.code = ERROR;
		}
	}
	return s;
}

/**
 * free up the space used in tracking previously queried results
 *
 */
status clear_res_list() {
	status ret;
	if (NULL != res_hash_list) {
		log_info("clearing the res_hash_list");
		Result *tmp, *res;
		HASH_ITER(hh, res_hash_list, res, tmp) {            
			if (NULL != res) {
				HASH_DEL(res_hash_list, res);
				free((void *)res->res_name);
				free(res->token);
				free(res);
			}
		}
		res_hash_list = NULL;
	}
	ret.code = OK;
	return ret;
}

/**
 * Prepare the db_operator for execution, get the Table and Column for select,
 * get the Result for fetch and tuple
 * By preparing the query, this function is responsible for taking all data needed
 * for the query into main memory
 * query:   the cmd string
 * d:       the matched dsl
 * op:      address of the db_operator 
 */ 
status query_prepare(const char* query, dsl* d, db_operator* op) {
	status s;

	if (SELECT_COL_CMD == d->g) { 
		// TODO: combine the selcet_col_cmd  and select_pre_cmd together 
		char* str_cpy = malloc(strlen(query) + 1);
		strncpy(str_cpy, query, strlen(query) + 1);
		char* pos_name = strtok(str_cpy, eq_sign);

		char* pos_var = malloc(sizeof(char) * (strlen(pos_name) + 1));
		strncpy(pos_var, pos_name, strlen(pos_name) + 1); 
		strtok(NULL, open_paren);

		// This gives us everything inside the '(' ')'
		char* args = strtok(NULL, close_paren);

		Table  *tmp_tbl = NULL;
		Column *tmp_col = NULL;
		args = prepare_col(args, &tmp_tbl, &tmp_col);
		// // This gives us <col_var>
		// char* col_var = strtok(args, comma);

		char *low_str = strtok(args, comma);
		// char* low_str = strtok(NULL, comma);
		int low = 0;
		if (NULL != low_str) {
			low = atoi(low_str);
		}
		else {
			s.code = WRONG_FORMAT;
			log_err("wrong select format");
			return s;
		}
		(void) low;

		char* high_str  = strtok(NULL, comma);
		int high = 0;
		if (NULL != high_str) {
			high = atoi(high_str);
		}
		else {
			s.code = WRONG_FORMAT;
			log_err("wrong select format");
			return s;   
		}
		(void) high;
		log_info("%s=select(%s,%d,%d)", pos_var, tmp_col->name, low, high);

		op->type = SELECT_COL;
		op->tables = malloc(sizeof(Tbl_ptr));
		op->tables[0] = tmp_tbl;

		(op->domain).cols = malloc(sizeof(Col_ptr));
		(op->domain).cols[0] = tmp_col;

		op->pos1 = NULL;
		op->pos2 = NULL;
		op->value1 = NULL;
		op->value2 = NULL;

		// Compose the comparator
		op->c = malloc(sizeof(comparator) * 2);
		(op->c[0]).p_val = low;
		(op->c[0]).type = GREATER_THAN | EQUAL;
		(op->c[0]).mode = AND;
		(op->c[1]).p_val = high;
		(op->c[1]).type = LESS_THAN;
		(op->c[1]).mode = NONE;
		(op->c[0]).next_comparator = &(op->c[1]);
		(op->c[1]).next_comparator = NULL;

		// Keep track of the name for further refering
		op->res_name = pos_var;

		// Cleanups
		free(str_cpy);
		s.code = OK;
		return s;
	}
	else if (SELECT_PRE_CMD == d->g) {
		char* str_cpy = malloc(strlen(query) + 1);
		strncpy(str_cpy, query, strlen(query) + 1);
		char* res_name = strtok(str_cpy, eq_sign);

		char* pos_var = malloc(sizeof(char) * (strlen(res_name) + 1));
		strncpy(pos_var, res_name, strlen(res_name) + 1); 
		strtok(NULL, open_paren);

		// This gives us everything inside the '(' ')'
		char* args = strtok(NULL, close_paren);

		// This gives us <col_var> 
		char* col_var = strtok(args, comma);

		// This gives us <posn_vec>
		char* posn_vec = strtok(NULL, comma);

		char* low_str = strtok(NULL, comma);
		int low = 0;
		if (NULL != low_str) {
			low = atoi(low_str);
		}
		else {
			s.code = WRONG_FORMAT;
			log_err("wrong select format");
			return s;
		}
		(void) low;
		char* high_str  = strtok(NULL, comma);
		int high = 0;
		if (NULL != high_str) {
			high = atoi(high_str);
		}
		else {
			s.code = WRONG_FORMAT;
			log_err("wrong select format");
			return s;   
		}
		(void) high;

		log_info("%s=select(%s,%s,%d,%d)\n", pos_var, posn_vec, col_var, low, high);

		// Grab the position list
		Result *tmp_pos = NULL;
		s = grab_result(posn_vec, &tmp_pos);

		if (OK != s.code) {
			log_err("cannot grab the position!\n");
			return s;
		}

		// Grab the sub column value list
		Result *tmp_val = NULL;
		s = grab_result(col_var, &tmp_val);
		if (OK != s.code) {
			log_err("cannot grab the value!\n");
			return s;
		}

		op->type = SELECT_PRE;
		op->tables = NULL;

		(op->domain).res = malloc(sizeof(Res_ptr));
		(op->domain).res[0] = tmp_val;
		op->position = tmp_pos;

		op->pos1 = NULL;
		op->pos2 = NULL;
		op->value1 = NULL;
		op->value2 = NULL;

		// Compose the comparator
		op->c = malloc(sizeof(comparator) * 2);
		(op->c[0]).p_val = low;
		(op->c[0]).type = GREATER_THAN | EQUAL;
		(op->c[0]).mode = AND;
		(op->c[1]).p_val = high;
		(op->c[1]).type = LESS_THAN;
		(op->c[1]).mode = NONE;
		(op->c[0]).next_comparator = &(op->c[1]);
		(op->c[1]).next_comparator = NULL;

		// Keep track of the name for further refering
		op->res_name = pos_var;

		// Cleanups
		free(str_cpy);
		s.code = OK;
		return s;
	}
	else if (FETCH_CMD == d->g) {
		
		char* str_cpy = malloc(strlen(query) + 1);
		strncpy(str_cpy, query, strlen(query) + 1);
		char* res_name = strtok(str_cpy, eq_sign);

		char* val_var = malloc(sizeof(char) * (strlen(res_name) + 1));
		strncpy(val_var, res_name, strlen(res_name) + 1); 
		strtok(NULL, open_paren);

		// This gives us everything inside the '(' ')'
		char* args = strtok(NULL, close_paren);

		// // THis gives us <col_var>
		// char* col_var = strtok(args, comma);

		// // This gives us <vec_pos>
		// char* vec_pos = strtok(NULL, comma);
		Table  *tmp_tbl = NULL;
		Column *tmp_col = NULL;
		Result *tmp_pos = NULL;
		args = prepare_col(args, &tmp_tbl, &tmp_col);
		args = prepare_res(args, &tmp_pos);

		log_info("%s=fetch(%s,%s)",val_var, tmp_col->name, tmp_pos->res_name);

		op->type = FETCH;
		op->tables = NULL;          // no need to record table

		(op->domain).cols = malloc(sizeof(Col_ptr));
		(op->domain).cols[0] = tmp_col;
		op->position = tmp_pos;

		op->pos1 = NULL;
		op->pos2 = NULL;
		op->value1 = NULL;
		op->value2 = NULL;
		op->c = NULL;

		// Keep track of the name for further refering
		op->res_name = val_var;
		
		// Cleanups
		free(str_cpy);
		s.code = OK;
		return s;
	}
	else if (TUPLE_CMD == d->g) {
		char* str_cpy = malloc(strlen(query) + 1);
		strncpy(str_cpy, query, strlen(query) + 1);
	
		// This gives us <col_var>  
		strtok(str_cpy, open_paren);
		char* col_var = strtok(NULL, close_paren);

		Result* tmp_res = NULL;
		s = grab_result(col_var, &tmp_res);

		if (OK != s.code) {
			log_err("tuple object not found\n");
			return s;
		}

		(op->domain).res = malloc(sizeof(Res_ptr));
		(op->domain).res[0] = tmp_res;

		op->type = TUPLE;
		op->tables = NULL;
		op->position = NULL;
		op->pos1 = NULL;
		op->pos2 = NULL;
		op->value1 = NULL;
		op->value2 = NULL;
		op->res_name = NULL;
		op->c = NULL;
		s.code = OK;
		return s;
	}
	else if (DELETE_CMD == d->g) {
		char* str_cpy = malloc(strlen(query) + 1);
		strncpy(str_cpy, query, strlen(query) + 1);
		strtok(str_cpy, open_paren);

		// This gives us everything inside the '(' ')'
		char* args = strtok(NULL, close_paren);

		Table *tmp_tbl = NULL;
		Column *tmp_col = NULL;
		Result *tmp_pos = NULL;
		args = prepare_col(args, &tmp_tbl, &tmp_col);
		args = prepare_res(args, &tmp_pos);

		op->type = DELETE;
		op->tables = malloc(sizeof(Tbl_ptr));
		op->tables[0] = tmp_tbl;
		// TODO: position1 should be used to delete, 
		// yet it should be a dynamic array? tmp_pos
		op->pos1 = NULL;
		op->pos2 = NULL;
		op->value1 = NULL;
		op->value2 = NULL;
		op->res_name = NULL;

		return s;

	}
	else if (INSERT_CMD == d->g) {
		// TODO: parse the insert command and prepare the db_op
	}
	else if (UPDATE_CMD == d->g) {
		// TODO: parse the update command and prepare the db_op
	}
	else {
		s.code = ERROR;
		return s;
	}

	// Should return earlier
	s.code = ERROR;
	return s;
}

/**
 * execute the query with the db_operator and put the output in results
 * op:      pointer to a db_operator storing detail of the operation
 * results: address of a Result to store the output
 */
status query_execute(db_operator* op, Result** results) {
	status s;
	switch (op->type) {
		case SELECT_COL: {
			s = col_scan(op->c, (op->domain).cols[0], (op->tables[0])->length, 
				results);
			if (OK != s.code) {
				// Something Wrong
				return s;
			}
			(*results)->res_name = op->res_name;
			HASH_ADD_KEYPTR(hh, res_hash_list, ((*results)->res_name), 
				strlen((*results)->res_name), *results);
			break;
		}
		case SELECT_PRE: {
			s = col_scan_with_pos(op->c, (op->domain).res[0], op->position, 
				results);
			if (OK != s.code) {
				// Something Wroing
				return s;
			}
			(*results)->res_name = op->res_name;
			HASH_ADD_KEYPTR(hh, res_hash_list, ((*results)->res_name), 
				strlen((*results)->res_name), *results);
			break;
		}
		case FETCH: {
			s = fetch_val((op->domain).cols[0], op->position, results);
			if (OK != s.code) {
				// Something Wrong 
				return s;
			}
			(*results)->res_name = op->res_name;
			HASH_ADD_KEYPTR(hh, res_hash_list, ((*results)->res_name), 
				strlen((*results)->res_name), *results);
			break;
		}
		default:
			break;
	}
	s.code = ERROR;
	return s;
}


/**
 * Comparasion used in SELECT operation
 * f:   pointer to the comparator containing all conditions
 * val: the integet to qualify
 * Return ture if val is qualified
 */
bool compare(comparator *f, int val){
	bool res = false, undone = true, cur_res;
	Junction pre_junc = NONE;
	comparator *tmp_f = f;
	while (undone && NULL != tmp_f) {
		if (((val < tmp_f->p_val) && (tmp_f->type & LESS_THAN)) 
			|| ((val == tmp_f->p_val) && (tmp_f->type & EQUAL))
			|| ((val > tmp_f->p_val) && (tmp_f->type & GREATER_THAN))) {
			cur_res = true;
		}
		else {
			cur_res = false;
		}
		switch (pre_junc) {
			case NONE: {
				res = cur_res;  
				break;
			}
			case AND: {
				res &= cur_res;
				break;
			}
			case OR: {
				res |= cur_res;
				break;
			}
		}

		if (NONE != tmp_f->mode) {
			pre_junc = tmp_f->mode;
			tmp_f = tmp_f->next_comparator;
		}
		else {
			tmp_f = NULL;
			undone = false;
		}
	}
	return res;
}

/**
 * load the Column data from disk with length len
 * col: the Column structure to store the data
 * len: length of the Column to be loaded
 * BUFFERSIZE: the macro used here is exactually the size of the fs buffer size
 */
status load_column4disk(Column *col, size_t len) {
	status s;
	static char dataprefix[] = "data/";
	char *colfile = NULL;
	int namelen = strlen(col->name);
	colfile = malloc(sizeof(char) * (6 + namelen));
	strncpy(colfile, dataprefix, 6);
	strncat(colfile, col->name, namelen);
	log_info("loading column %s from disks!\n", colfile);

	col->data = darray_create(len);
	FILE *fp = fopen(colfile, "rb");
	free(colfile);

	size_t read = len;
	char *buffer = malloc(sizeof(int) * BUFFERSIZE);
	if (NULL == fp) {
		log_info("cannot open file!\n");
		s.code = ERROR;
		return s;
	}

	// Try to read 1k integers at a time
	for (size_t i = 0; i <= (len - 1) / BUFFERSIZE; i++) {
		if (read < BUFFERSIZE) {
			size_t r = 0;
			while(r < read) {
				size_t t = fread(buffer + sizeof(int) * r, sizeof(int), read - r, fp);
				if (0 == t) {
					darray_destory(col->data); free(buffer); col->data = NULL;  
					if (ferror(fp)) {
						log_err("error loading from disks!\n");
						// darray_destory(col->data); free(buffer); col->data = NULL;
						fclose(fp);
						s.code = ERROR;
						return s;
					}
					else {
						fclose(fp);
						s.code = ERROR;
						return s;
					}
				}
				r += t;
			}
			fclose(fp);
			// append data into column
			darray_vec_push(col->data, buffer, read);
			free(buffer);
		}
		else {
			size_t r = 0;
			while (r < BUFFERSIZE) {
				size_t t = fread(buffer + sizeof(int) * r, sizeof(int), BUFFERSIZE - r, fp);
				if (0 == t) {
					darray_destory(col->data); 
					free(buffer); 
					col->data = NULL;
					if (ferror(fp) || feof(fp)) {
						log_err("error loading from disks!\n");
						// darray_destory(col->data);
						// free(buffer);
						// col->data = NULL;
						fclose(fp);
						s.code = ERROR;
						return s;
					}
					else {
						fclose(fp);
						s.code = ERROR;
						return s;   
					}
				}
				r += t;
			}
			read -= BUFFERSIZE;
			// append data into column
			darray_vec_push(col->data, buffer, BUFFERSIZE);
		}
	}

	s.code = OK;
	return s;
}

/** 
 * scan the whole Column with conditions
 * f:   pointer to the comparator containing all conditions
 * col: pointer to the Column to be scaned
 * r:   address of a pointer to Result to put to the output
 */
status col_scan(comparator *f, Column *col, size_t len, Result **r) {
	status s;
	if (NULL != col) {
		*r = malloc(sizeof(Result));
		(*r)->token = NULL;
		(*r)->num_tuples = 0;
		for (size_t i = 0; i < len; i++) {
			if (compare(f, (col->data)->content[i])) {
				// TODO: Results storing needs improving later
				(*r)->num_tuples++;
				(*r)->token = realloc((*r)->token, (*r)->num_tuples * sizeof(Payload));
				(*r)->token[(*r)->num_tuples - 1].pos = i;
			}
		}
		s.code = OK;
		log_info("col_scan %zu tuple qualified", (*r)->num_tuples);
		for (size_t ii = 0; ii < (*r)->num_tuples; ii++) {
			log_info("pos selected %zu ", (*r)->token[ii].pos);
		}
		return s;
	}

	s.code = ERROR;
	return s;
}

/**
 * scan a Column at some positions specified
 * f:   pointer to the comparator containing all conditions
 * col: pointer to the Column to be scaned
 * pos: pointer to a Result as positions specified
 * r:   address of a pointer to Result to put to the output
 */
status col_scan_with_pos(comparator *f, Result *res, Result *pos, Result **r) {
	status s;
	if (NULL != res && NULL != pos) {
		*r = malloc(sizeof(Result));
		(*r)->token = NULL;
		(*r)->num_tuples = 0;
		size_t i = 0;
		while (i < pos->num_tuples) {
			// if (compare(f, (col->data)->content[pos->token[i].pos])) {
			if (compare(f, res->token[i].val)) {    
				// TODO: Results storing needs improving later
				(*r)->num_tuples++;
				(*r)->token = realloc(((*r)->token), (*r)->num_tuples * sizeof(Payload));
				(*r)->token[(*r)->num_tuples - 1].pos = pos->token[i].pos;
			}
			i++;
		}
		s.code = OK;
		log_info("col_scan_with_pos %zu tuple qualified\n", (*r)->num_tuples);
		return s;
	}

	s.code = ERROR;
	return s;
}

/**
 * fetch the value from a Column at positions specified
 * col: pointer to the Column to be scaned
 * pos: pointer to a Result as positions specified
 * r:   address of a pointer to Result to put to the output
 */
status fetch_val(Column *col, Result *pos, Result **r) {
	status s;
	if (NULL != col && NULL != pos) {
		*r = malloc(sizeof(Result));
		(*r)->num_tuples = pos->num_tuples;
		size_t i = 0;
		(*r)->token = malloc((*r)->num_tuples * sizeof(Payload));
		log_info("fetched data:\n");
		while (i < pos->num_tuples) {
			(*r)->token[i].val = (col->data)->content[pos->token[i].pos];
			log_info(" %d ", (*r)->token[i].val);
			i++;
		}
		s.code = OK;
		return s;
	}
	s.code = ERROR;
	return s;
}

/**
 * tuple the values requested
 * return a char array of integers separated by \n
 */
char* tuple(db_operator *query) {
	if (NULL != query && NULL != (query->domain).res[0]) {
		Result *r = (query->domain).res[0];
		size_t total_space = 1024;
		size_t used_space = 1, i = 0;
		char *ret = NULL;
		ret = realloc(ret, sizeof(char) * total_space);
		ret[0] = '\0';
		
		for (i = 0; i < r->num_tuples; i++) {
			char num[20];
			sprintf(num, "%d\n", r->token[i].val);
			used_space += strlen(num);
			if (used_space > total_space){
				total_space *= 2;   
				ret = realloc(ret, sizeof(char) * total_space);
			}
			strncat(ret, num, strlen(num));
		}
		return ret;     
	}
	return  NULL;
}

/**
 * scan a whole partition in a Column with partition id
 * col:     pointer to the Column to be scaned
 * part_id: partition id specified
 * r:       address of a pointer to Result to put to the output
 */
status scan_partition(Column *col, int part_id, Result **r) {
	status s;
	s.code = ERROR;
	if (NULL != col && part_id < col->partitionCount) {
		int pos_s = col->p_pos[part_id];
		int pos_e = col->p_pos[part_id + 1]; 
		*r = malloc(sizeof(Result));
		(*r)->num_tuples = pos_e - pos_s;
		int j = 0;
		(*r)->token = malloc((*r)->num_tuples * sizeof(Payload));
		for (int i = pos_s; i < pos_e; i++) {
			(*r)->token[j++].pos = i;
		}
		s.code = OK;
	}
	return s;
}

/**
 * scan a partition in a Column with greaterThan condition
 * col:     pointer to the Column to be scaned
 * val:     all qualified integers should be greater than val
 * part_id: partition id specified
 * r:       address of a pointer to Result to put to the output
 */
status scan_partition_greaterThan(Column *col, int val, int part_id, Result **r) {
	status s;
	s.code = ERROR;
	if (NULL != col && part_id < col->partitionCount) {
		int pos_s;
		if (1 == col->partitionCount) {
			pos_s = 0;
		}
		else {
			pos_s = col->p_pos[part_id - 1] + 1;
		}
		int pos_e = col->p_pos[part_id];
		*r = malloc(sizeof(Result));
		(*r)->num_tuples = 0;
		int j = 0;
		for (int i = pos_s; i < pos_e; i++) {
			if (col->data->content[i] > val) {
				(*r)->token = realloc((*r)->token, sizeof(Payload) * (j + 1));
				(*r)->token[j].pos = i;
				j++;
			}
		}
		(*r)->num_tuples = j + 1;
		s.code = OK;
	}
	return s;
}

/**
 * scan a partition in a Column with lessThan condition
 * col:     pointer to the Column to be scaned
 * val:     all qualified integers should be less than val
 * part_id: partition id specified
 * r:       address of a pointer to Result to put to the output
 */
status scan_partition_lessThan(Column *col, int val, int part_id, Result **r) {
	status s;
	s.code = ERROR;
	if (NULL != col && part_id < col->partitionCount) {
		// start position
		int pos_s = 0;
		if (1 == col->partitionCount) {
			pos_s = 0;
		}
		else {
			pos_s = col->p_pos[part_id - 1] + 1;
		}
		// end position
		int pos_e = col->p_pos[part_id];
		*r = malloc(sizeof(Result));
		(*r)->num_tuples = 0;
		int j = 0;
		for (int i = pos_s; i < pos_e; i++) {
			if (col->data->content[i] < val) {
				(*r)->token = realloc((*r)->token, sizeof(Payload) *(j + 1));
				(*r)->token[j].pos = i;
				j++;
			}
		}
		(*r)->num_tuples = j + 1;
		s.code = OK;
	}
	return s;
}

/**
 * scan a partition in a Column with equal condition
 * col:     pointer to the Column to be scaned
 * val:     all qualified integers should be equal to val
 * part_id: partition id specified
 * r:       address of a pointer to Result to put to the output
 */
status scan_partition_pointQuery(Column *col, int val, int part_id, Result **r) {
	status s;
	s.code = ERROR;
	if (NULL != col && part_id < col->partitionCount) {
		int pos_s = col->p_pos[part_id];
		int pos_e = col->p_pos[part_id + 1]; 
		*r = malloc(sizeof(Result));
		(*r)->num_tuples = 0;
		int j = 0;
		for (int i = pos_s; i < pos_e; i++) {
			if (col->data->content[i] == val) {
				(*r)->token = realloc((*r)->token, sizeof(Payload) * (j + 1));
				(*r)->token[j].pos = i;
				j++;
			}
		}
		(*r)->num_tuples = j + 1;
		s.code = OK;
	}
	return s;
}

/**
 * prepare the Column together with the Table it belongs to
 * args:	a char array to parse
 * tbl:		address of a Table to hold the Table pointer 
 * col:		address of a Table to hold the Column pointer 	
 */
char *prepare_col(char *args, Table **tbl, Column **col) {
	char *col_var = strtok(args, comma);

	unsigned int i =0, flag = 0;
	while('\0' != col_var[i]) {
		if ('.' == col_var[i]) {
			flag++;
			if (2 == flag) {		// Find the second '.'
				break;
			}
		}
		i++;
	}

	char* tbl_var = malloc(sizeof(char) * (i + 1));
	strncpy(tbl_var, col_var, i);
	tbl_var[i] = '\0';
	printf("table name in select %s\n", tbl_var);
	
	Table* tmp_tbl = NULL;
	status s = grab_table(tbl_var, &tmp_tbl);
	if (OK != s.code) {
		log_err("cannot grab the table!");
		return NULL;
	}
	free(tbl_var);
	*tbl = tmp_tbl;	

	// Grab the column
	Column *tmp_col = NULL;
	s = grab_column(col_var, &tmp_col);

	if (OK != s.code) {
		log_err("cannot grab the column!\n");
		return NULL;
	}

	// Data of the column might not be in main memory
	if (NULL != tmp_tbl && NULL == tmp_col->data && 0 != tmp_tbl->length) {
		load_column4disk(tmp_col, tmp_tbl->length);
	}
	*col = tmp_col;

	return col_var + strlen(col_var) + 1;	
}

/**
 * prepare the Result together with the Table it belongs to
 * args:	a char array to parse
 * res:		address of a Table to hold the Result pointer 
 */
char *prepare_res(char *args , Result **res) {
	char *vec_res = strtok(args, comma);

	// Grab the position list
	Result *tmp_res = NULL;
	status s = grab_result(vec_res, &tmp_res);
	if (OK != s.code) {
		log_err("cannot grab the results!\n");
		return NULL;
	}
	*res = tmp_res;
	return vec_res + strlen(vec_res) + 1;
}

