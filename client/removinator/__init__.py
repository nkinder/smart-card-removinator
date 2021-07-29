from removinator import *

def cli():
    import argparse
    from removinator import removinator

    parser = argparse.ArgumentParser()
    parser.add_argument('command', nargs='*', default=['get_status'])
    args = parser.parse_args()

    conn = removinator.Removinator()

    if args.command[0] == 'insert_card':
        if len(args.command) != 2:
            print('insert_card command must specify ONE slot number')
        else:
            print('Switching slot to {}'.format(args.command[1]))
            conn.insert_card(int(args.command[1]))
    elif args.command[0] == 'remove_card':
        print('Removing card from current slot')
        conn.remove_card()
    elif args.command[0] == 'get_status':
        print(conn.get_status())
    else:
        print('Invalid command selected...running get_status instead')
        print(conn.get_status())
