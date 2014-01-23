#ifndef __MMI_H_
#define __MMI_H_

#define MAX_MMI_ITEMS			40
#define MAX_MMI_TEXT_LEN		255
#define MAX_MMI_CHOICE_TEXT_LEN		255

typedef struct {
	int choice_nb;
	char title[MAX_MMI_TEXT_LEN];
	char subtitle[MAX_MMI_TEXT_LEN];
	char bottom[MAX_MMI_TEXT_LEN];
	char choice_item[MAX_MMI_ITEMS][MAX_MMI_CHOICE_TEXT_LEN];
} MMI_MENU_LIST_INFO;

typedef struct {
	int blind;
	int answerlen;
	char enquiryText[MAX_MMI_TEXT_LEN];
} MMI_ENQUIRY_INFO;

/* compat */
#define enguiryText	 enquiryText
#define MMI_ENGUIRY_INFO MMI_ENQUIRY_INFO

#endif // __MMI_H_

