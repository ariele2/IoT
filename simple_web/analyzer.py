#%%
from asyncio import events
import pandas as pd
import os
import gspread
from df2gspread import df2gspread as d2g
from google.oauth2.service_account import Credentials
import argparse


def already_analyzed(analyze_name):
    if os.path.exists(os.path.join('csv_analyzes', analyze_name)):
        return os.path.join('csv_analyzes', analyze_name)
    return ''


#%%
def analyze(report_path):
    if not os.path.exists(os.path.abspath(report_path)):
        print(f'Cannot locate given path: "{report_path}"')

    analyze_name = f'analyze{report_path[report_path.rfind("report")+len("report"):]}'
    analyzed_path =  already_analyzed(analyze_name)
    if analyzed_path:
        return analyzed_path

    df = pd.read_csv(report_path)
    time_col = df.loc[:, 'Time'].values
    day_col = df.loc[:, 'Day'].values
    even = df.loc[:, 'Event'].values
    number_of_people = []
    for ev in even:
        if ev == 'ERROR' or ev =="NO" or ev =="OUT":
            number_of_people.append(0)
        elif ev == "YES" or ev == "IN" :
            number_of_people.append(1)
        else:
            number_of_people.append(ev)
    df.insert(df.columns,"Occupied", number_of_people)
    df.drop('Day', inplace=True, axis=1)    # remove day column
    df.drop('Time', inplace=True, axis=1)   # remove time column
    new_day_col = []
    for d in day_col:
        day = d.split('/')
        new_day_col.append(f'{day[1]}/{day[0]}/{day[2]}')   # create day in american format
    date_time_col = [f'{d} {t}' for d,t in zip(new_day_col, time_col)]  # array of day&time 
    df.insert(0, 'Date&Time', date_time_col)    # insert the date&time column to the table

    df.to_csv(os.path.join('csv_analyzes', analyze_name), index=False)  # create a csv inside the analyzes directory
    return os.path.join('csv_analyzes', analyze_name)
    

def upload_sheet(analyze_csv_path):
    df = pd.read_csv(analyze_csv_path)
    creds_file_path = 'C:\\Users\\ariel\\Desktop\\IoTIDEs\\IoTgit\\IoT\\simple_web\\sheets_creds.json'
    if not os.path.exists(creds_file_path):
        print('Cannot locate credentials!')
    # scopes = [
    #     'https://www.googleapis.com/auth/spreadsheets',
    #     'https://www.googleapis.com/auth/drive'
    # ]
    # credentials = Credentials.from_service_account_file(creds_file_path, scopes=scopes)
    gc = gspread.service_account(creds_file_path)
    sheet_key = '1FaH-OZLBkhZ8WSGn3gLZB135cX_dCkPJ0SrSH8aYk6Y'
    # wks_name = 'Master'
    # sheet = gc.open_by_key('10xEKnW6bO8odBNbPii3npuZrlES4v9jkFhqxwLFLgFw')
    content = open(analyze_csv_path, 'r').readlines()
    content_lines = []
    for line in content:
        new_row = [a.strip() for a in line.split(',')]
        content_lines.append(new_row)
    master = gc.open_by_key(sheet_key).sheet1
    master.clear()
    master.update(content_lines, value_input_option='USER_ENTERED')

    # sheet = gc.import_csv(sheet_key, content)
    # sheet = d2g.upload(df, gfile=sheet_key, wks_name=wks_name, df_size=True, clean=True)
    print('Done!')

def create_analyzed_sheet(report_path):
    analyze_csv_path = analyze(report_path)
    upload_sheet(analyze_csv_path)


#%%
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse analyzer args')
    parser.add_argument('-report', type=str, default='')
    args=parser.parse_args()
    create_analyzed_sheet(args.report)