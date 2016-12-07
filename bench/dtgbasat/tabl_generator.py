#!/usr/bin/env python3

import csv
import re
from pylatex import Document, Package, Tabular, MultiColumn
from pylatex.utils import bold, NoEscape
from pylatex.base_classes import CommandBase, Arguments
from pylatex.base_classes.command import Parameters


class ColorArgument(Parameters):
    """
        A class implementing custom arguments formating for LaTeX command.
    """
    def dumps(self):
        params = self._list_args_kwargs()
        if len(params) <= 0 or len(params) > 2:
            return ''
        string = '{' + params[0] + '} ' + params[1]
        return string


class SetColor(CommandBase):
    """
        A class representing a LaTeX command used to colorize table's cells.
    """
    _latex_name = 'cellcolor'

    def dumps(self):
        arguments = self.arguments.dumps()
        res = '\\' + self._latex_name + arguments[1:-1]
        return res


class DefineColor(CommandBase):
    """
        A class representing a LaTeX command used to define a color.
    """
    _latex_name = 'definecolor'


def ne(string):
    """
        A wrapper around pylatex class NoEscape. It helps to tell pylatex to
        not escape a string.

        Be careful:
                ne(string) + ne(string) = ne(string)
            but
                ne(string) + simple_string = simple_string
    """
    return NoEscape(string)


def dba_st_cmp_dtgba_st(pattern, line):
    """
        Function that checks if minDTGBA.st * (minDTGBA.acc + 1) < minDBA.st.
        It should not be the case normally!
    """
    dba_st_id = get_st_index_of(pattern, line)
    dba_st = line[dba_st_id]
    dtgba_v = 0
    dtgba_acc = 0
    dba_v = 0
    if '-' not in dba_st and '!' not in dba_st:
        dba_v = int(dba_st)
        if '-' not in line[dba_st_id + 10] \
                and '!' not in line[dba_st_id + 10]:
            dtgba_v = int(line[dba_st_id + 10])
            dtgba_acc = int(line[dba_st_id + 13])
            return dtgba_v * (dtgba_acc + 1) < dba_v
    return False


def get_last_successful(n, category, pattern):
    """
        A function used to get the last automaton size from satlog file.
    """
    try:
        log = open(str(n) + '.' + category + '.' + pattern
                   + '.satlog', 'r')
        log_csv = csv.reader(log)
        for line in log_csv:
            min_val = line[1]
        return ', $\\le$ ' + min_val
    except Exception:
        return ''


def get_st_index_of(pattern, line):
    """
        A function used to get the first column of each benchmarked tool.
    """
    size = len(line)
    for i in range(0, size):
        if pattern in line[i]:
            return i + 1
    raise ValueError('\'' + pattern + '\' is not found in log files')


def check_all_st_are_eq(line, pattern):
    """
        A function that:
        - retrieve all column values that are just after any column containing
        the pattern.
        - check that all those values are exactly the same.
    """
    size = len(line)
    l = []
    for i in range(0, size):
        if pattern in line[i]:
            if '-' not in line[i + 1] and '!' not in line[i + 1] \
                    and 'n/a' not in line[i + 1]:
                try:
                    int(i + 1)
                except Exception as e:
                    print(e)
                    exit()
                l.append(line[i + 1])

    return all(x == l[0] for x in l)


def add_other_cols(row, line, l, d):
    """
        A function used to add all columns that dynamically depend on the
        config.bench file.
    """
    n = int(line[-1])
    category = ['DBA', 'DTGBA']

    dba_t = []
    dtgba_t = []
    dba_st = int(line[13])

    all_dba_st_eq = check_all_st_are_eq(line, 'minDBA')
    all_dtgba_st_eq = check_all_st_are_eq(line, 'minDTGBA')

    for instance in l:
        for elt in category:
            if 'DBA' in elt:
                width = 3
            elif 'DTGBA' in elt:
                width = 4
            st_id = get_st_index_of('min' + elt + '.' + instance, line)
            if '-' in line[st_id]:
                s = ne(get_last_successful(n, elt, instance))
                row.append(MultiColumn(width, align='c|',
                           data=ne('(killed ') + s + ne(')')))
            elif '!' in line[st_id]:
                s = ne(get_last_successful(n, elt, instance))
                row.append(MultiColumn(width, align='c|',
                           data=ne('(intmax ') + s + ne(')')))
            else:
                cur_st = int(line[st_id])

                if 'DBA' in elt and \
                        dba_st_cmp_dtgba_st('min' + elt + '.' + instance,
                                            line):
                    row.append(SetColor(
                        arguments=ColorArgument('Purpl', str(cur_st))))
                elif ((not all_dba_st_eq) and 'DBA' in elt) \
                        or ((not all_dtgba_st_eq) and 'DTGBA' in elt) \
                        or cur_st > dba_st:
                    row.append(SetColor(
                        arguments=ColorArgument('Red', str(cur_st))))
                elif cur_st < dba_st:
                    row.append(SetColor(
                        arguments=ColorArgument('Gray', str(cur_st))))
                else:
                    row.append(str(cur_st))

                row.append(line[st_id + 2])  # st + 2 = trans
                time = '%.2f' % round(float(line[st_id + 7]), 2)
                if width > 3:
                    row.append(line[st_id + 3])
                    dtgba_t.append(time)
                else:
                    dba_t.append(time)
                row.append(time)

    try:
        dba = min(float(x) for x in dba_t)
    except ValueError:
        dba = -1
    try:
        dtgba = min(float(x) for x in dtgba_t)
    except ValueError:
        dtgba = -1
    return dba, dtgba


def get_dra_st(line, c):
    """
        Get state of DRA.
    """
    for i in range(0, len(line)):
        if 'DRA' in line[i]:
            if 'n/a' in line[i + 1]:
                return ''
            else:
                return str(int(line[i + 1]) - 1 + int(c))


def get_type(type_f):
    """
        A function used to categorized each formula.
    """
    tmp = ''
    if 'trad' in type_f:
        tmp = 'T'
    elif 'TCONG' in type_f:
        tmp = 'P'
    elif 'DRA' in type_f:
        tmp = 'R'
    elif 'WDBA' in type_f:
        tmp = 'W'
    else:
        tmp = type_f
    return tmp


def val(line, v):
    """
        A function used to retrieve any integer located at line[v].
    """
    try:
        res = int(line[v])
    except Exception as e:
        if '-' in line[v]:
            return 1
        else:
            print(e)
            exit()
    return res


def all_aut_are_det(line, l):
    """
        A function that check that all automaton produced are determinist.
    """
    if val(line, 8) == 0 or val(line, 18) == 0:
        return False
    size = len(l)
    for i in range(1, size + 1):
        if val(line, 18 + 10 * i) == 0:
            return False
    return True


def clean_formula(f):
    """
        A function used to clean any formula.
    """
    res = '$'
    f_iter = iter(f)
    for it in f_iter:
        if it == '&':
            res += '\\land '
        elif it == '|':
            res += '\\lor '
        elif it == '!':
            res += '\\bar '
        elif it == '-' and next(f_iter, it) == '>':
            res += '\\rightarrow '
        elif it == '<' and next(f_iter, it) == '-' and next(f_iter, it) == '>':
            res += '\\leftrightarrow '
        else:
            res += it
    return ne(res + '$')


def add_static_cols(row, line, l):
    """
        A function used to add the 14 first static columns. Those columns don't
        depend on the config.bench file.
    """
    f = clean_formula(line[0])  # TODO: define math operators for formula
    m = line[1]
    f_type = line[2]
    c = line[9]
    if all_aut_are_det(line, l):
        c_str = line[9]
    else:
        c_str = SetColor(arguments=ColorArgument('Red', line[9]))
    dtgba_st = line[3]
    dtgba_tr = line[5]
    dtgba_acc = line[6]
    dtgba_time = '%.2f' % round(float(line[10]), 2)
    dba_st = line[13]
    dba_tr = line[15]
    dba_time = line[20]

    row.append(f)  # formula
    row.append(m)
    row.append(get_type(f_type))  # trad -> T, TCONG -> P, DRA -> R, WDBA -> W
    row.append(c_str)  # is complete or not
    row.append(get_dra_st(line, c))
    row.append(dtgba_st)
    row.append(dtgba_tr)
    row.append(dtgba_acc)
    row.append(dtgba_time)
    row.append(dba_st)
    row.append(dba_tr)
    if '-' in dba_time or '!' in dba_time:
        row.append(dba_time)
    else:
        row.append('%.2f' % round(float(dba_time), 2))

    # DBAminimizer
    length = len(line)
    for i in range(0, length):
        if 'dbaminimizer' in line[i]:
            if '-' in line[i + 1]:
                row.append(MultiColumn(2, align='|c', data='(killed)'))
            elif 'n/a' in line[i + 1]:
                row.append('')
                row.append('')
            else:
                minimizer_st = int(line[i + 1]) - 1 + int(c)
                if minimizer_st < int(dba_st):
                    row.append(SetColor(
                        arguments=ColorArgument('Gray', str(minimizer_st))))
                elif minimizer_st > int(dba_st):
                    row.append(SetColor(
                        arguments=ColorArgument('Red', str(minimizer_st))))
                else:
                    row.append(minimizer_st)
                row.append('%.2f' % round(float(line[i + 2]), 2))


def next_bench_considering_all(line, index):
    """
        A function used to get the index of the next benchmark. It takes into
        account '(killed)' MultiColumns...
    """
    try:
        line[index + 7]
    except:
        return index + 7

    if is_not_MultiColumn(line, index):
        if is_not_MultiColumn(line, index + 3):
            return index + 7
        else:
            return index + 4
    else:
        if is_not_MultiColumn(line, index + 1):
            return index + 5
        else:
            return index + 2


def is_eq(to_be_compared, val):
    """
        Check that two values are almost equal (5% tolerance).
    """
    try:
        return to_be_compared / val <= 1.05     # to_... is always >= val
    except ZeroDivisionError:
        return is_eq(to_be_compared + 1, val + 1)


def is_not_MultiColumn(line, index):
    """
        Check that the type(line[index]) is not MultiColumn.
    """
    try:
        return type(line[index]) is not MultiColumn
    except IndexError as e:
        print(e)
        exit()


def get_first_mindba(line):
    """
        A function used to get the index of the first benchmark (just after
        the static columns).
    """
    if type(line[12]) is MultiColumn:
        return 13
    return 14


def get_lines(l, d):
    """
        Entry point for parsing the csv file. It returns all lines that will
        be displayed. After this function, no more treatment are done on datas.
    """
    all_l = []
    best_dba_l = []
    best_dtgba_l = []
    ifile = open('good.csv', 'r')
    reader = csv.reader(ifile)
    for line in reader:
        row = []
        add_static_cols(row, line, l)          # 14 first columns
        dba_t, dtgba_t = add_other_cols(row, line, l, d)     # All the rest
        all_l.append(row)
        best_dba_l.append(dba_t)
        best_dtgba_l.append(dtgba_t)

    all_lines_length = len(all_l)
    for i in range(0, all_lines_length):
        index = get_first_mindba(all_l[i])
        size = len(all_l[i])
        while index < size:
            if best_dba_l[i] != -1:
                if is_not_MultiColumn(all_l[i], index):
                    if is_eq(float(all_l[i][index + 2]), best_dba_l[i]):
                        all_l[i][index + 2] = SetColor(
                            arguments=ColorArgument('Green',
                                                    str(best_dba_l[i])))

            if best_dtgba_l[i] != -1:
                if is_not_MultiColumn(all_l[i], index):
                    if is_not_MultiColumn(all_l[i], index + 3)\
                            and is_eq(float(all_l[i][index + 6]),
                                      best_dtgba_l[i]):
                            all_l[i][index + 6] = SetColor(
                                arguments=ColorArgument('Yelw',
                                                        str(best_dtgba_l[i])))
                else:
                    if is_not_MultiColumn(all_l[i], index + 1)\
                            and is_eq(float(all_l[i][index + 4]),
                                      best_dtgba_l[i]):
                            all_l[i][index + 4] = SetColor(
                                arguments=ColorArgument('Yelw',
                                                        str(best_dtgba_l[i])))

            index = next_bench_considering_all(all_l[i], index)
    return all_l, best_dba_l, best_dtgba_l


def write_header(table, l, d):
    """
        Function that write the first lines of the document.
    """
    # Static datas
    data_row1 = ne('Column ') + bold('type') + \
        ne(' shows how the initial det. aut. was obtained: T = translation'
           ' produces DTGBA; W = WDBA minimization works; P = powerset '
           'construction transforms TBA to DTBA; R = DRA to DBA.')
    data_row2 = ne('Column ') + bold('C.') + \
        ne(' tells whether the output automaton is complete: rejecting '
           'sink states are always omitted (add 1 state when C=0 if you '
           'want the size of the complete automaton).')
    row3 = [MultiColumn(14)]
    row4 = ['', '', '', '', 'DRA',
            MultiColumn(4, align='|c', data='DTGBA'),
            MultiColumn(3, align='|c', data='DBA'),
            MultiColumn(2, align='|c',
                        data=ne('DBA\\footnotesize minimizer'))]
    row5 = ['formula', 'm', 'type', 'C.', 'st.', 'st.', 'tr.',
            'acc.', 'time', 'st.', 'tr.', 'time', 'st.', 'time']

    # Datas that depends on the configuration file
    for elt in l:
        row3.append(MultiColumn(7, align='|c', data=d[elt]))
        row4.append(MultiColumn(3, align='|c', data='minDBA'))
        row4.append(MultiColumn(4, align='|c', data='minDTGBA'))
        row5.extend(['st.', 'tr.', 'time', 'st.', 'tr.', 'acc.', 'time'])

    # Add the first 5 lines of the document.
    n = 14 + len(l) * 7
    table.add_row((MultiColumn(n, align='c', data=data_row1),))
    table.add_row((MultiColumn(n, align='c', data=data_row2),))
    table.add_row((MultiColumn(n, align='l', data=''),))  # add empty line
    table.add_row(tuple(row3))
    table.add_row(tuple(row4))
    table.add_row(tuple(row5))
    table.add_hline()


def add_fmt(nfields):
    """
        Function used to define the table's format depending on config.bench
        file.
    """
    tmp = '|lrcr|r|rrrr|rrr|rr|'
    for i in range(0, nfields):
        tmp += 'rrr|rrrr|'
    return tmp


def parse_config():
    """
        Function used to parse the config.bench file.
    """
    l = []  # list of keys
    d = {}  # dictionnary
    with open('config.bench', 'r') as f:
        lines = f.readlines()
        for line in lines:
            if (line[0] == '#'):
                continue
            key = re.search('(.+?):', line).group(1)
            val = re.search(':(.+?)$', line).group(1)
            l.append(key)
            d[key] = val
    return l, d


def write_resume(best_dba_l, best_dtgba_l, l):
    res = []
    # Header
    header = ['']
    for elt in l:
        header.append(elt)
    res.append(header)

    '''
    # Body
    ifile = open('good.csv', 'r')
    reader = csv.reader(ifile)
    for line in reader:
        for instance in l:
            for elt in category:
                if 'DBA' in elt:
                    width = 3
                elif 'DTGBA' in elt:
                    width = 4
                st_id = get_st_index_of('min' + elt + '.' + instance, line)
    '''
    return res


def main():
    # First we parse the configuration file
    l, d = parse_config()

    # Now let's create the doc and its table
    doc = Document(documentclass='standalone')
    doc.packages.append(Package('amsmath'))
    doc.packages.append(Package('color'))
    doc.packages.append(Package('colortbl'))
    doc2 = Document(documentclass='standalone')

    # Declare Colors
    doc.append(DefineColor(
        arguments=Arguments('Gray', 'rgb', '0.7, 0.7, 0.7')))
    doc.append(DefineColor(arguments=Arguments('Green', 'rgb', '0.4, 1, 0.4')))
    doc.append(DefineColor(arguments=Arguments('Red', 'rgb', '0.8, 0, 0')))
    doc.append(DefineColor(arguments=Arguments('Yelw', 'rgb', '1, 0.98, 0.4')))
    doc.append(DefineColor(arguments=Arguments('Purpl', 'rgb', '1, 0.6, 1')))

    # Create Table with format
    table = Tabular(add_fmt(len(l)))
    table2 = Tabular(len(l))

    # Write header (first 5 lines)
    write_header(table, l, d)

    # Write lines
    lines, best_dba_l, best_dtgba_l = get_lines(l, d)
    for line in lines:
        table.add_row(tuple(line))

    # Write resume
    lines_resume = get_resume(best_dba_l, best_dtgba_l, l)
    for line in lines_resume:
        table2.add_row(tuple(line))

    # Output PDF
    doc.append(table)
    doc2.append(table2)
    doc.generate_pdf('res')
    doc2.generate_pdf('resume')


if __name__ == '__main__':
    main()
